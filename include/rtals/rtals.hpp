#pragma once

#include <filesystem>
#include <memory>

namespace rtals {

/// @brief Manages the lifetime and UI automation of an Ableton Live session (process + main window)
/// @details
///   - Launches the target program
///   - Waits for a visible top-level window of that PID, explicitly skipping the transient splash
///   - Uses common dialogs to open/save project files by sending keystrokes and setting the
///     filename edit control text
///   - Destructor posts WM_CLOSE to the app and attempts to auto-confirm common prompts with Enter
/// @warning Automation relies on the app owning the foreground, other apps stealing focus may
///          cause operations to fail sporadically
struct session {
    session() = delete;
    session(const session& other) = delete;
    session& operator=(const session& other) = delete;
    session(session&& other) = default;
    session& operator=(session&& other) = default;

    /// @brief Attempts graceful app shutdown and releases OS handles
    /// @details
    ///   Posts WM_CLOSE to the main window (and other top-levels of the PID), waits briefly
    ///   for the process to exit, and auto-confirms common prompts by sending Enter
    ~session() noexcept;

    /// @brief Launch the target program and capture its main window
    /// @param program Absolute path to the Ableton Live executable to launch
    /// @details
    ///   Uses CreateProcessW, then polls all top-level windows of the child PID, ignoring the first
    ///   transient splash and returning when the “real” main window appears and is visible
    /// @throws std::runtime_error if the process cannot be created or no main window is found
    ///         within the internal timeout
    /// @note Path may use either '\\' or '/' separators, they are normalized internally
    session(const std::filesystem::path& program);

    /// @brief Load a project into the running app via the Open dialog (Ctrl+O)
    /// @param project Path to a '.als' project to open
    /// @details
    ///   Brings the app to the foreground, sends Ctrl+O, locates the common file dialog’s
    ///   filename edit control, sets the absolute path, and confirms (Open/Enter)
    /// @throws std::runtime_error if the Open dialog or its filename control cannot be found
    /// @note Path may use either '\\' or '/' separators, they are normalized internally
    void load_project(const std::filesystem::path& project);

    /// @brief Save the current project (Ctrl+S)
    /// @details
    ///   Sends Ctrl+S to the foreground app. If the set is untitled, Live may surface a Save As
    ///   dialog; the implementation presses Enter to accept the current suggestion, and will
    ///   auto-confirm overwrite/stop-audio prompts with Enter
    /// @throws std::runtime_error only in severe state errors (e.g., no main window)
    void save_project();

    /// @brief Save the current project under a new path (Ctrl+Shift+S) using the Save As dialog
    /// @param project Destination path (folder + filename)
    /// @details
    ///   Brings the app to the foreground, sends Ctrl+Shift+S, locates the dialog’s filename edit,
    ///   sets the provided path, confirms Save, and auto-accepts overwrite prompts with Enter
    /// @throws std::runtime_error if the Save As dialog or its filename control cannot be found
    /// @note Path may use either '\\' or '/' separators, they are normalized internally
    void save_project_as(const std::filesystem::path& project);

private:
    std::shared_ptr<struct session_impl> _impl;
};

}