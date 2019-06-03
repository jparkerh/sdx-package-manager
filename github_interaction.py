from git import Repo
import os, stat, time
import json
import shutil

# clone a repo from the given url into the tmp folder
def clone_repo_tmp(repo_location):
    clear_tmp_folder()
    time.sleep(2)

    print("cloning repo")
    repo = Repo.clone_from(repo_location, "./tmp")
    repo.close()
    print("Repo cloned")

# move any jsons
def move_jsons_to_dir():
    print("Moving json files")

    #Find all jsons in tmp
    for root, dirs, files in os.walk("./tmp"):
        for file in files:
            if file.endswith(".json"):
                rel_dir = os.path.relpath("./")
                rel_file = os.path.join(rel_dir, file)
                shutil.move(os.path.join("./tmp", rel_file), os.path.join("./repos/" + rel_file))

    clear_tmp_folder()

# move all named files cloned from repo (in tmp) into the modules directory
def move_kernel_files_to_modules(file_list, subdir):
    print("Moving kernel files")

    for entry in file_list:
        rel_dir = os.path.relpath("./tmp")
        rel_file = os.path.join(rel_dir, entry)
        try:
            os.makedirs("./modules/" + subdir)
        except FileExistsError:
            pass
        print(rel_file)
        shutil.move(rel_file, os.path.join("./modules/" + subdir + "/" + entry))

    clear_tmp_folder()

# pull down a source for json repo targets
def source_repo_listing(listing_location):
    clone_repo_tmp(listing_location)
    move_jsons_to_dir()

# pull in all of the repositories into dictionaries from the repo jsons
def source_repo_info():
    for root, dirs, files in os.walk("./repos"):
        for file in files:
            if file.endswith(".json"):
                with open("./repos/" + file, "r") as read_file:
                    data = json.load(read_file)
                    for subcat in data:
                        for source_loc in data[subcat]:
                            print(source_loc + ": " + data[subcat][source_loc]['description'])

def clear_tmp_folder():
    print("starting delete")
    for root, dirs, files in os.walk("./tmp"):
        for file in files:
            rel_dir = os.path.relpath(root)
            rel_file = os.path.join(rel_dir, file)
            # print(rel_file)
            os.chmod(rel_file, stat.S_IRWXU| stat.S_IRWXG| stat.S_IRWXO)

    print("clear directory")
    if os.path.exists("./tmp"):
        shutil.rmtree("./tmp")

# move necessary files from repo listings
def source_all_repo_files(repo_name):
    for root, dirs, files in os.walk("./repos"):
        for file in files:
            if file.endswith(".json"):
                with open("./repos/" + file, "r") as read_file:
                    data = json.load(read_file)
                    for subcat in data:
                        for source_loc in data[subcat]:
                            if source_loc == repo_name:
                                clone_repo_tmp(data[subcat][source_loc]['repo-url'])
                                move_kernel_files_to_modules(data[subcat][source_loc]['kernel-file-list'], source_loc)

source_repo_listing("https://github.com/parker-xilinx/kernel-repo.git")
# clone_repo_tmp("https://github.com/Xilinx/xfopencv")
source_all_repo_files("xfopencv")
source_all_repo_files("2d_filter_xfopencv")
