
#include <signal.h>
//#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#include <linux/limits.h>
#endif

#ifdef _WIN32
#include <windows.h>
#define stat _stat
#define popen _popen
#define pclose _pclose
//#define sleep _sleep
#endif


#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>

//https://stackoverflow.com/questions/6806173/subclass-inherit-standard-containers
template <class T> class uniquevector: private std::vector<T>
{
private:
	typedef std::vector<T> base_vector;

public:
	typedef typename base_vector::size_type       size_type;
	typedef typename base_vector::iterator        iterator;
	typedef typename base_vector::const_iterator  const_iterator;

	using base_vector::operator[];

	using base_vector::begin;
	using base_vector::clear;
	using base_vector::end;
	using base_vector::erase;
	using base_vector::push_back;
	using base_vector::reserve;
	using base_vector::resize;
	using base_vector::size;
	
	void add (const T& val)
	{
		for (auto it = this->begin(); it != this->end(); ++it)
			if (*it == val) return;
		this->push_back(val);
	}
};

struct filerec {
	std::string filename;
	std::string targetfilename;
	uniquevector<std::string> dependencies;
};

bool operator==(const filerec& lhs, const filerec& rhs)
{
    if (lhs.filename == rhs.filename) return true;
	return false;
}

//main data structures:
std::map<std::string, std::time_t> files;
std::vector<filerec> scadfiles;
std::map<std::string, std::string> env;

//signal (Ctrl-C) handler:
static void signal_function(int signo) {
	std::cout << "Ctrl-C, terminating..." << std::endl;
	exit(EXIT_SUCCESS);
}

int countchar(std::string s, char c)
{
	int count = 0;
	for (int i=0; i<s.size(); i++) {
		if (s[i] == c) count++;
	}
	return count;
}

std::vector<std::string> bifurcate(std::string strg, char c)
{
	std::vector<std::string> result;
	if (countchar(strg, c) == 0) {
		result.push_back(strg);
	}
	else {
		std::size_t eq;
		eq = strg.find_first_of(c);
		result.push_back(strg.substr(0,eq));
		result.push_back(strg.substr(eq+1));
	}
	return result;
}

//https://stackoverflow.com/questions/5878775/how-to-find-and-replace-string
void replace_all(std::string& s, std::string const& toReplace, std::string const& replaceWith
) {
    std::string buf;
    std::size_t pos = 0;
    std::size_t prevPos;

    // Reserves rough estimate of final size of string.
    buf.reserve(s.size());

    while (true) {
        prevPos = pos;
        pos = s.find(toReplace, pos);
        if (pos == std::string::npos)
            break;
        buf.append(s, prevPos, pos - prevPos);
        buf += replaceWith;
        pos += toReplace.size();
    }

    buf.append(s, prevPos, s.size() - prevPos);
    s.swap(buf);
}

//https://stackoverflow.com/questions/2203159/is-there-a-c-equivalent-to-getcwd
std::string getcwd_string( void ) {
	char buff[PATH_MAX];
	getcwd( buff, PATH_MAX );
	std::string cwd( buff );
	return cwd;
}

//https://stackoverflow.com/questions/40504281/c-how-to-check-the-last-modified-time-of-a-file
std::time_t get_modtime(std::string f)
{
	std::filesystem::path p(f);
	if (!std::filesystem::exists(p)) return 0;
	struct stat result;
	if(stat(f.c_str(), &result)==0) return result.st_mtime;
	return 0;
}

std::string get_absolutepath(std::string f)
{
	std::filesystem::path p = std::filesystem::absolute(std::filesystem::path(f)).lexically_normal();
	return p.string();
}

std::string get_relativepath(std::string f)
{
	std::filesystem::path p = std::filesystem::relative(std::filesystem::path(f)).lexically_normal();
	std::string s = p.string();
	//replace_all(s, "\\", "\\\\");
	//replace_all(s, "\", "/");
	std::replace( s.begin(), s.end(), '\\', '/');
	return s;
}

std::vector<std::string> scad_dependencies(std::string scadfile)
{
	std::ifstream infile(scadfile);
	std::string line;
	std::vector<std::string> deps;
	deps.clear();
	
	while (std::getline(infile, line))
	{
		if (line.find("//") != std::string::npos) line.erase(line.find("//"));
		if ((line.find("use") != std::string::npos) | (line.find("include") != std::string::npos)) {
			if (line.find_first_of('<') == std::string::npos | line.find_first_of('>') == std::string::npos) continue;
			if (line.find("<") != std::string::npos) line.erase(0, line.find_first_of('<')+1);
			if (line.find(">") != std::string::npos) line.erase(line.find_first_of('>'));
			deps.push_back(line);
		}
	}
	infile.close();
	
	return deps;
}

void load_env(std::string filename)
{
	std::ifstream infile(filename);
	std::string line;
	
	while (std::getline(infile, line))
	{
		if (line.find_first_of('=') != std::string::npos) {
			std::vector<std::string> t = bifurcate(line, '=');
			if (t.size() >= 2) env[t[0]] = t[1];
		}
	}
	infile.close();
}

//void add_dependencies(filerec &f)
void add_dependencies(std::string f, uniquevector<std::string> &d)
{
	//std::cout << "add_dependencies: " << f << std::endl;
	std::string cwd = getcwd_string();
	std::filesystem::path p = std::filesystem::absolute(std::filesystem::path(f)).lexically_normal();
	chdir(p.parent_path().string().c_str());
	
	std::vector<std::string> deps = scad_dependencies(f);
	//std::cout << "\t" << p.string() << std::endl;
	if (p.is_absolute()) files[p.string()] = get_modtime(p.string());
	d.add(p.string());
	for (std::vector<std::string>::iterator it = deps.begin(); it != deps.end(); ++it)
		add_dependencies(*it, d);
	chdir(cwd.c_str());
}

uniquevector<filerec> build_files()
{
	uniquevector<filerec> b;
	for (std::vector<filerec>::iterator it = scadfiles.begin(); it != scadfiles.end(); ++it) {
		std::time_t target_modtime =  get_modtime((*it).targetfilename);
		for (uniquevector<std::string>::iterator jt = (*it).dependencies.begin(); jt != (*it).dependencies.end();  ++jt) {
			std::time_t dep_modtime =  get_modtime(*jt);
			if (dep_modtime > target_modtime) {
				filerec t;
				t.filename = (*it).filename;
				t.targetfilename = (*it).targetfilename;
				b.add(t);
				//std::cout << *jt << ": " << dep_modtime << "  " << (*it).targetfilename << ": " << target_modtime << std::endl;   
			}
		}
	}
	return b;
}

void print_allthestuff(std::map<std::string, std::vector<std::filesystem::path>> s)
{
	for (std::map<std::string, std::vector<std::filesystem::path>>::iterator st= s.begin(); st != s.end(); ++ st) {
		std::cout << st->first << ":" <<std::endl;
		for (std::vector<std::filesystem::path>::iterator vt = (st->second).begin(); vt != (st->second).end(); ++vt) {
			std::cout << "\t" << (*vt).string() << std::endl;
		}
	}
}

std::map<std::string, std::vector<std::filesystem::path>> collect_files(int argc, char **argv)
{
	std::map<std::string, std::vector<std::filesystem::path>> files;
	for (int i=1; i<argc; i++) {
		std::filesystem::path p(std::string(argv[i]));
		if (std::filesystem::exists(p)) 
			files[p.parent_path().string()].push_back(p);
	}	
	return files;
}

void print_files(uniquevector<std::string> f)
{
	for (uniquevector<std::string>::iterator it = f.begin(); it != f.end(); ++it)
		std::cout << *it << std::endl;
}

void print_scadfiles()
{
	for (std::vector<filerec>::iterator it = scadfiles.begin(); it != scadfiles.end(); ++it) {
		std::cout << (*it).filename << ": " << (*it).targetfilename << std::endl;
		for (uniquevector<std::string>::iterator jt = (*it).dependencies.begin(); jt != (*it).dependencies.end();  ++jt)
			std::cout << "\t" << *jt << std::endl;
	}
}

void print_environment()
{
	for (std::map<std::string, std::string>::iterator it = env.begin(); it != env.end(); ++it)
		std::cout << it->first << ": " << it->second << std::endl;
}

void run_command(std::string scadfile, std::string stlfile)
{
	std::string cmdstring = env["COMMAND"];
	replace_all(cmdstring, "SCADFILE", scadfile);
	replace_all(cmdstring, "STLFILE", stlfile);
			
	std::cout << cmdstring << std::endl;
					
	FILE *f;
	char p[PATH_MAX];
		
	f = popen(cmdstring.c_str(), "r");
				
	while (fgets(p, PATH_MAX, f) != NULL) {
		printf("%s", p);
		fflush(stdout);
	}
				
	pclose(f);
}


void scad_monitor()
{
	while (true) {
		//collect scad files with changes/dep changes:
		uniquevector<filerec> r = build_files();
			
		if (r.size() > 0) {
			// for each touched scad file, build a command line and run it with popen()
			for (uniquevector<filerec>::iterator it = r.begin(); it != r.end(); ++it) {
				run_command((*it).filename, (*it).targetfilename);
			}
			std::cout << std::endl << "back to file monitoring..." << std::endl << std::endl;
		}
		
#ifdef _WIN32
		Sleep(2000);
#else
		sleep(2);
#endif
	}
}

void scad_build()
{
	//collect scad files with changes/dep changes:
	uniquevector<filerec> r = build_files();

	if (r.size() > 0) {
		// for each touched scad file, build a command line and run it with popen()
		for (uniquevector<filerec>::iterator it = r.begin(); it != r.end(); ++it) {
			run_command((*it).filename, (*it).targetfilename);
		}
	}
	else std::cout << "Nothing to build." << std::endl;
}

void scad_makefile()
{
	std::cout << "#Makefile generated by scadmake" << std::endl << std::endl;
	std::string toptarget = "all:";
	for (std::vector<filerec>::iterator it = scadfiles.begin(); it != scadfiles.end(); ++it)
		toptarget.append(" "+ get_relativepath((*it).targetfilename));
	std::cout << toptarget << std::endl << std::endl;
	
	for (std::vector<filerec>::iterator it = scadfiles.begin(); it != scadfiles.end(); ++it) {
		std::cout << get_relativepath((*it).targetfilename) << ": ";
		for (uniquevector<std::string>::iterator jt = (*it).dependencies.begin(); jt != (*it).dependencies.end();  ++jt) {
			//if ((*it).filename == (*jt)) continue;
			std::cout << " " << get_relativepath(*jt);
		}
		std::string cmdstring = env["COMMAND"];
		replace_all(cmdstring, "SCADFILE", get_relativepath((*it).filename));
		replace_all(cmdstring, "STLFILE", get_relativepath((*it).targetfilename));
		std::cout << std::endl << "\t" + cmdstring << std::endl << std::endl;
	}
}

void age_diff(int argc, char **argv)
{
	std::string hilight, norm;
	hilight = "\033[1;31m";
	norm = "\033[0m";
	std::filesystem::path a(argv[2]);
	std::string ap = a.parent_path().string();  //first wildcard path
	
	std::filesystem::path b(argv[argc-1]);
	std::string bp = b.parent_path().string();  //second wildcard path
	std::string bx = b.extension().string();  //second wildcard extension

	std::map<std::string, std::vector<std::filesystem::path>> files = collect_files(argc, argv);
	
	for (std::vector<std::filesystem::path>::iterator it = files[ap].begin(); it != files[ap].end(); ++it) {
		time_t f1 = get_modtime((*it).string());
		std::string f1name = (*it).filename().string();
		
		time_t f2 = 0;
		std::string f2name = "";
		for (std::vector<std::filesystem::path>::iterator jt = files[bp].begin(); jt != files[bp].end(); ++jt) {
			if ((*jt).stem() == (*it).stem()) {
				f2 = get_modtime((*jt).string());
				f2name = (*jt).filename().string();
				break;
			}
		}
		if (f1 > f2)
			std::cout << hilight << f1name << norm << "\t" << f2name << std::endl;
		else
			std::cout << f1name << "\t" << hilight << f2name << norm << std::endl;
	}
}

int main(int argc, char **argv)
{
	
	//set up the ctrl-c signal handler
	if (signal(SIGINT, signal_function) == SIG_ERR) { //register Ctrl-C handler
		std::cerr << "An error occurred while setting the Ctrl-C signal handler." << std::endl;
		return EXIT_FAILURE;
	}
	
	//set defaults:
	env["SCADDIR"] = ".";
	env["STLDIR"] = ".";
	env["EXT"] = ".stl";
	
	//load make file:
	load_env("SCADMakefile");
	
	//append directory veriables with '/' if not present:
	if (env["SCADDIR"][env["SCADDIR"].size()-1] != '/') env["SCADDIR"].append(std::string("/"));
	if (env["STLDIR"][env["STLDIR"].size()-1] != '/') env["STLDIR"].append(std::string("/"));
	
	//collect the file modification times for the .scad files (move to add_dependencies)
    for (const auto & p :  std::filesystem::directory_iterator(env["SCADDIR"]))
        if (p.path().extension() == ".scad") {
			filerec f;
			f.filename = std::filesystem::absolute(p.path().lexically_normal()).string();
			f.targetfilename = get_absolutepath(env["STLDIR"] + p.path().stem().string() + env["EXT"]);
			f.dependencies.clear();
			scadfiles.push_back(f);
		}

	std::string cwd = getcwd_string();
	chdir(env["SCADDIR"].c_str());
	
	//populate the dependencies for each .scad file
	for (int i=0; i<scadfiles.size(); i++) {
		add_dependencies(scadfiles[i].filename, scadfiles[i].dependencies);
	}
	chdir(cwd.c_str());
	
	
	if (argc >= 2) { 
		if (std::string(argv[1]) == "-h") {
			std::cout << "Usage: scadmake [-m|-M|-a path1 path2]" << std::endl;
			std::cout << "\t" << "-m: monitor mode, read SCADMakefile and monitor for target changes, Ctrl-c to exit" << std::endl;
			std::cout << "\t" << "-M: generate Makefile for unix make" << std::endl;
			std::cout << "\t" << "-a: age diff equivalent files in two directories" << std::endl;
			std::cout << "\t" << "No switch: read SCADMakefile and make old targets" << std::endl;
		}
		else if (std::string(argv[1]) == "-m") {
			std::cout << "SCAD directory: " << env["SCADDIR"] << std::endl;
			std::cout << "Monitoring for build changes..." << std::endl;
			scad_monitor();
		}
		else if (std::string(argv[1]) == "-M") {
			scad_makefile();
		}
		else if (std::string(argv[1]) == "-a") {
			age_diff(argc, argv);
		}
	}
	else {
		std::cout << "SCAD directory: " << env["SCADDIR"] << std::endl;
		scad_build();
	}
}

