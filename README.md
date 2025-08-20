# rtals

Instrumentation of the Ableton Live executable. This project provides functionnality for launching Ableton Live process, loading projects, saving projects and saving project as, passing through dialogs when needed. Documentation for the `rtals::session` data structure can be found directly in the [rtals/rtals.hpp](include/rtals/rtals.hpp) header.

### Usage

Use `rtals::session _session("path_to_ableton.exe");` to launch the executable. Executable will be closed when `_session` goes out of scope.

Use `_session.load_project("path_to_my_project.als");` to load a project and pass through dialogs

Use `_session.save_project();` to save a project

Use `_session.save_project_as("path_to_my_project.als");` to save a project and pass through dialogs

### Supported versions
Should work on any version > 9 but tested on:
- 9.7.7
- 12.0.0
