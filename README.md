# rtals

Instrumentation of the Ableton Live executable from C++17. This project provides functionnality for launching Ableton Live process, loading projects, saving projects and saving project as, passing through dialogs when needed. Documentation for the `rtals::session` data structure can be found directly in the [rtals/rtals.hpp](include/rtals/rtals.hpp) header. Win32 implementation but MacOS planned.

### Usage

```cpp
rtals::session _session("path_to_ableton.exe");         // launch the executable
_session.load_project("path_to_my_project.als");        // load a project and pass through dialogs
_session.save_project();                                // save a project
_session.save_project_as("path_to_my_project.als");     // save a project and pass through dialogs
```

### Supported versions
Should work on any version > 9 but tested on:
- 9.7.7
- 12.0.0
