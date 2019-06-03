<h3>Proto sdx kernel/package manager</h3>

Uses JSON files describing HLS repositories to pull in sources/includes for a given resource into modules directory. Modules directory can then be added by absolute path to SDx/HLS project. 

Use the json file in the kernel-repo repository as a reference of how to add repository listing targets to the package manager. JSON listing needs to include description, URL of the github for the repository... etc.

Working:
Github integration to pull repo listings from github
Use JSON files in sourced listings to pull pertinent files into the modules folder

Needs:
Source HLS tcl file to run build with managed cpp/c files and includes to be added automatically

