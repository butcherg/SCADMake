# SCADMake
A program to make OpenSCAD projects, in the manner of unix Make

As one assembles a group of OpenSCAD scripts to model parts of an assembly, it becomes a bit tedious to keep rendering and exporting files for printing.  Programmers might try to use unix make to streamline the proces in the same manner done for their programming projects, but the levels of dependencies make it challenging to write a Makefile with the specific dependencies for each top-level .scad script.  Once you get it set up, it's really cool to just run "make" and watch it run OpenSCAD only on the scripts with changes, or their dependencies.

SCADMake is a command-line program that works a lot like GNU make, but is tailored to OpenSCAD scripts. It (currently) uses a SCADMakefile to specify the directory with the SCAD scripts, and the directory where the .stl files will be deposited. A OpenSCAD command line is also specified in SCADMakefile.  However, no targets are required, as SCADMake reads all the scripts in the SCAD directory, extracts the dependencies, and builds a tree for subsequent use.  Its behavior is then much like unix make, where it compares the modify times of the target file (.stl) and script and its dependencies, and runs the command to remake the target file if any are newer.

It also will run in a "monitor" mode, -m in the scadmake invocation will set the program to run continuously, and when a script changes the dependent scripts are run to produce the respective targets.  Ctrl-C exits scadmake in this mode.

If you'd rather use a unix make, scadmake will generate a Makefile with the -M switch.  It produces a Makefile with individual targets for each top-level .scad script, with paths relative to the current working directory.  So, make sure you run it in the directory where you want the Makefile to reside.

## Building

scadmake is a C++ program, requires at least C++17 for the filesystem library.  It has no other dependencies; monitor mode is just an infinite loop, so there's no OS-specific file watcher.

```
$ make
```
You'll need to copy the executable to an appropriate directory by hand,  I may do a cmake build system later.

## Using
I've organized my current project as follows:
```
scad/
stl/
lib/
```
scad/ is where I put the .scad files that produce a printable part.  lib/ is where I put all the other .scad files, things like utilities.scad, where I put various modules that are used in many places, and globals.scad, where I define coordinates for placing parts in an integration script.  stl/ is where the .stl files reside that correspond to the .scad files in scad/.  This is where I have the SCADMakefile (and a regular Makefile, just for grins), and here is where I do all the make-ing.  For monitor mode, I open a shell, cd to the stl/ directory, and run scadmake -m.  As I edit scripts, when I save one the change is detected at the next loop iteration and the appropriate top-level .stl is re-generated.

## Limitations
 - Right now, scadmake uses unix-style paths, with '/' as the separator.  On Windows, I can run such with bash in msys2.  I'll figure out how to use Windows cmd/powershell later.
 - The unix Makefile generated is hard-coded to the project state at the time of generation.  When you add or delete files, you'll need to run scadmake -M again.


## License

I chose the MIT license, seemed appropriate for a one-off hack...
