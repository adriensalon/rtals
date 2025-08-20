#include <rtals/rtals.hpp>

#include <iostream>
#include <thread>

#include <windows.h>

static void send_ctrl_o()
{
    INPUT _in[4] = {};
    _in[0].type = INPUT_KEYBOARD;
    _in[0].ki.wVk = VK_CONTROL;
    _in[1].type = INPUT_KEYBOARD;
    _in[1].ki.wVk = 'O';
    _in[2] = _in[1];
    _in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    _in[3] = _in[0];
    _in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, _in, sizeof(INPUT));
}

static void send_ctrl_s()
{
    INPUT _in[4] = {};
    _in[0].type = INPUT_KEYBOARD;
    _in[0].ki.wVk = VK_CONTROL;
    _in[1].type = INPUT_KEYBOARD;
    _in[1].ki.wVk = 'S';
    _in[2] = _in[1];
    _in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    _in[3] = _in[0];
    _in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, _in, sizeof(INPUT));
}

static void send_ctrl_shift_s()
{
    INPUT _in[6] = {};
    _in[0].type = INPUT_KEYBOARD;
    _in[0].ki.wVk = VK_CONTROL;
    _in[1].type = INPUT_KEYBOARD;
    _in[1].ki.wVk = VK_SHIFT;
    _in[2].type = INPUT_KEYBOARD;
    _in[2].ki.wVk = 'S';
    _in[3] = _in[2];
    _in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    _in[4] = _in[1];
    _in[4].ki.dwFlags = KEYEVENTF_KEYUP;
    _in[5] = _in[0];
    _in[5].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(6, _in, sizeof(INPUT));
}

static void send_enter()
{
    INPUT _in[2] = {};
    _in[0].type = INPUT_KEYBOARD;
    _in[0].ki.wVk = VK_RETURN;
    _in[1] = _in[0];
    _in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, _in, sizeof(INPUT));
}

static HWND try_known_filename_ids(HWND dlg)
{
    // common in open
    if (HWND _h = GetDlgItem(dlg, 1148)) { // cmb13 / ComboBoxEx
        if (HWND _edit = FindWindowExW(_h, NULL, L"Edit", NULL)) {
            return _edit;
        }
        if (lstrcmpW(L"Edit", L"#dummy")) {
            return _h; // sometimes the ID is on the Edit itself
        }
    }

    // common in save as
    if (HWND _edit = GetDlgItem(dlg, 1152)) { // edt1 (File name)
        return _edit;
    }
    if (HWND _h = GetDlgItem(dlg, 1145)) { // cmb13 (alt ID on some builds)
        if (HWND _edit = FindWindowExW(_h, NULL, L"Edit", NULL)) {
            return _edit;
        }
    }

    return NULL;
}

static BOOL CALLBACK find_edit_cb(HWND hwnd, LPARAM lp)
{
    if (!IsWindowVisible(hwnd) || !IsWindowEnabled(hwnd)) {
        return TRUE;
    }
    wchar_t _classname[64];
    GetClassNameW(hwnd, _classname, 64);
    if (lstrcmpW(_classname, L"Edit") == 0) {
        *reinterpret_cast<HWND*>(lp) = hwnd;
        return FALSE; // stop
    }
    return TRUE; // continue
}

static HWND find_descendant_edit(HWND dlg)
{
    HWND _found_window = NULL;
    EnumChildWindows(dlg, find_edit_cb, reinterpret_cast<LPARAM>(&_found_window));
    return _found_window;
}

static HWND resolve_filename_edit(HWND dlg)
{
    if (HWND _edit = try_known_filename_ids(dlg)) {
        return _edit;
    }

    // some dialogs wrap controls in DirectUIHWND/ComboBoxEx32 trees
    // look for any ComboBoxEx32 then its Edit
    for (HWND _child_window = FindWindowExW(dlg, NULL, L"ComboBoxEx32", NULL); _child_window; _child_window = FindWindowExW(dlg, _child_window, L"ComboBoxEx32", NULL)) {
        if (HWND _edit = FindWindowExW(_child_window, NULL, L"Edit", NULL)) {
            return _edit;
        }
    }

    // any visible enabled Edit in the whole subtree
    return find_descendant_edit(dlg);
}

static HWND wait_dialog_of_process(HWND app, DWORD timeout_ms = 5000)
{
    DWORD _target_process_id = 0;
    GetWindowThreadProcessId(app, &_target_process_id);
    const DWORD _start = GetTickCount();
    while (GetTickCount() - _start < timeout_ms) {
        for (HWND _window = GetTopWindow(NULL); _window; _window = GetWindow(_window, GW_HWNDNEXT)) {
            DWORD _process_id = 0;
            GetWindowThreadProcessId(_window, &_process_id);
            if (_process_id == _target_process_id) {
                wchar_t _classname[64];
                GetClassNameW(_window, _classname, 64);
                if (lstrcmpW(_classname, L"#32770") == 0)
                    return _window;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return NULL;
}

static bool is_app_main_candidate(HWND w)
{
    if (!IsWindow(w)) {
        return false;
    }
    if (GetWindow(w, GW_OWNER) != NULL) {
        return false; // owned → likely splash/popup
    }
    LONG_PTR _ex = GetWindowLongPtrW(w, GWL_EXSTYLE);
    if (_ex & WS_EX_TOOLWINDOW) {
        return false; // tool windows ≠ main
    }
    LONG_PTR _st = GetWindowLongPtrW(w, GWL_STYLE);
    if (!(_st & WS_CAPTION)) {
        return false; // needs a frame/caption
    }
    // Optional: insist on sizable frames (many main windows have these)
    // if (!(_st & WS_THICKFRAME)) return false;
    return true;
}

static HWND wait_main_window(DWORD pid, DWORD timeout_ms = 30000, DWORD stable_ms = 800)
{
    DWORD _start = GetTickCount();
    HWND _current = NULL;
    DWORD _since = 0;

    while (GetTickCount() - _start < timeout_ms) {
        HWND _found = NULL;
        
        for (HWND _window = GetTopWindow(NULL); _window; _window = GetWindow(_window, GW_HWNDNEXT)) {
            DWORD _pid = 0;
            GetWindowThreadProcessId(_window, &_pid);
            if (_pid != pid) {
                continue;
            }
            if (!is_app_main_candidate(_window)) {
                continue; // no IsWindowVisible check
            }
            _found = _window;
            break; // pick the first suitable top-level
        }

        if (_found) {
            if (_found == _current) {
                if (_since && (GetTickCount() - _since >= stable_ms)) {
                    return _found; // persisted long enough so accept
                }
            } else {
                _current = _found;
                _since = GetTickCount();
            }
        } else {
            _current = NULL;
            _since = 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return NULL;
}

static HWND wait_foreground_dialog_of_process(HWND owner, DWORD timeout_ms = 6000)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(owner, &pid);
    DWORD start = GetTickCount();
    while (GetTickCount() - start < timeout_ms) {
        HWND fg = GetForegroundWindow();
        if (fg) {
            DWORD p = 0;
            GetWindowThreadProcessId(fg, &p);
            if (p == pid && IsWindowVisible(fg)) {
                wchar_t cls[64] = L"";
                GetClassNameW(fg, cls, 64);
                if (lstrcmpW(cls, L"#32770") == 0)
                    return fg; // classic dialog
                // Some shells briefly focus a wrapper; if it has an edit+Open/Save button, accept it.
                if (GetDlgItem(fg, 1148) || GetDlgItem(fg, IDOK))
                    return fg;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return NULL;
}

static HWND wait_save_dialog(HWND owner, DWORD timeout_ms = 10000)
{
    // try foreground-first (fast), then your process scan (broad).
    if (HWND _window = wait_foreground_dialog_of_process(owner, timeout_ms)) {
        return _window;
    }
    return wait_dialog_of_process(owner, timeout_ms);
}

static void post_close_to_all_windows_of(DWORD pid)
{
    for (HWND _window = GetTopWindow(NULL); _window; _window = GetWindow(_window, GW_HWNDNEXT)) {
        DWORD _pid = 0;
        GetWindowThreadProcessId(_window, &_pid);
        if (_pid == pid)
            PostMessageW(_window, WM_CLOSE, 0, 0);
    }
}

static bool wait_process_exit(HANDLE h, DWORD timeout_ms)
{
    return h && WaitForSingleObject(h, timeout_ms) == WAIT_OBJECT_0;
}

namespace rtals {

struct session_impl {
    HWND main_window = nullptr;
    HANDLE process_handle = nullptr;
    DWORD process_id = 0;
    bool is_project_loaded = false;
};

session::session(const std::filesystem::path& program)
{
    _impl = std::make_shared<session_impl>();
    std::wstring _command_line = L"\"" + program.wstring() + L"\"";
    STARTUPINFOW _startup_info = { sizeof(_startup_info) };
    PROCESS_INFORMATION _process_information {};

    if (!CreateProcessW(program.c_str(), _command_line.data(), NULL, NULL, FALSE, 0, NULL, NULL, &_startup_info, &_process_information)) {
        std::wcerr << L"Failed to start process: " << program << L"\n";
        return;
    }
    _impl->process_handle = _process_information.hProcess; // keep it for the dtor
    _impl->process_id = _process_information.dwProcessId;
    CloseHandle(_process_information.hThread);

    WaitForInputIdle(_impl->process_handle, 15000); // ignore failures it’s just a hint
    _impl->main_window = wait_main_window(_impl->process_id, 15000);
    if (!_impl->main_window) {
        std::wcerr << L"Window for process not found\n";
        CloseHandle(_impl->process_handle);
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::wcout << L"Found main window: " << _impl->main_window << L"\n";
}

session::~session() noexcept
{
    if (!_impl)
        return;

    // if we have no process handle just return
    HANDLE _handle = _impl->process_handle;
    if (!_handle)
        return;

    // ask the app to close nicely
    if (_impl->main_window && IsWindow(_impl->main_window)) {
        PostMessageW(_impl->main_window, WM_CLOSE, 0, 0);
    }
    // some apps have multiple top-levels (splash/tool windows), nudge all
    post_close_to_all_windows_of(_impl->process_id);

    // give it a moment to react
    if (wait_process_exit(_handle, 2000)) {
        CloseHandle(_handle);
        _impl->process_handle = nullptr;
        return;
    }

    if (HWND _dialog_window = wait_dialog_of_process(_impl->main_window, 1500)) {
        SetForegroundWindow(_dialog_window);
        send_enter(); // accept default (usually "Save"/"OK")
    }
    // TODO send_enter_if_ableton_foreground(_impl->main_window, 1500);

    // wait again after confirming
    if (wait_process_exit(_handle, 3000)) {
        CloseHandle(_handle);
        _impl->process_handle = nullptr;
        return;
    }

#ifdef RTALS_FORCE_KILL_ON_DESTRUCT
    TerminateProcess(h, 0);
    WaitForSingleObject(h, 2000);
#endif

    // release handle
    CloseHandle(_handle);
    _impl->process_handle = nullptr;
}

void session::load_project(const std::filesystem::path& project)
{
    if (_impl->is_project_loaded) {
        save_project();
    }

    SetForegroundWindow(_impl->main_window);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    send_ctrl_o();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    HWND _open_dialog = wait_dialog_of_process(_impl->main_window, 5000);
    if (!_open_dialog) {
        throw std::runtime_error("Open dialog not found");
    }

    HWND _filename_edit = resolve_filename_edit(_open_dialog);
    if (!_filename_edit) {
        throw std::runtime_error("Open filename edit not found");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    SetFocus(_filename_edit); // focus + set text
    std::filesystem::path _norm_project = project.lexically_normal();
    const wchar_t* _wproject = _norm_project.c_str();
    SendMessageW(_filename_edit, WM_SETTEXT, 0, (LPARAM)_wproject);

    SetForegroundWindow(_open_dialog); // activate the dialog and press enter (or click open: ID 1)
    HWND _open_button_window = GetDlgItem(_open_dialog, 1);
    if (_open_button_window) {
        SendMessageW(_open_button_window, BM_CLICK, 0, 0);
    } else {
        PostMessageW(_open_dialog, WM_KEYDOWN, VK_RETURN, 0);
        PostMessageW(_open_dialog, WM_KEYUP, VK_RETURN, 0);
    }

    _impl->is_project_loaded = true;
    std::cout << "Requested open: " << project << std::endl;
}

void session::save_project()
{
    SetForegroundWindow(_impl->main_window);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    send_ctrl_s();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

void session::save_project_as(const std::filesystem::path& project)
{
    SetForegroundWindow(_impl->main_window);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    send_ctrl_shift_s();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    HWND _saveas_dialog = wait_save_dialog(_impl->main_window, 10000);
    if (!_saveas_dialog) {
        throw std::runtime_error("Save As dialog not found");
    }

    HWND _filename_edit = resolve_filename_edit(_saveas_dialog);
    if (!_filename_edit) {
        throw std::runtime_error("Save filename edit not found");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    SetFocus(_filename_edit);
    std::filesystem::path _norm_project = project.lexically_normal();
    const wchar_t* _wproject = _norm_project.c_str();
    SendMessageW(_filename_edit, WM_SETTEXT, 0, (LPARAM)_wproject);

    SetForegroundWindow(_saveas_dialog); // press save (IDOK/1) or Enter
    if (HWND _enter_button = GetDlgItem(_saveas_dialog, 1)) {
        SendMessageW(_enter_button, BM_CLICK, 0, 0);
    } else {
        PostMessageW(_saveas_dialog, WM_KEYDOWN, VK_RETURN, 0);
        PostMessageW(_saveas_dialog, WM_KEYUP, VK_RETURN, 0);
    }
}

}
