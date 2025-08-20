#include <rtals/rtals.hpp>

#include <iostream>

int main()
{
    try {
        rtals::session _session("C:/ProgramData/Ableton/Live 9 Standard/Program/Ableton Live 9 Standard.exe");
        _session.load_project("C:/Users/adri/Desktop/Untitled Project/Untitled.als");
        _session.save_project();
        _session.save_project_as("C:/Users/adri/Desktop/Untitled2.als");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
}