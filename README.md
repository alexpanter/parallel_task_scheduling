# Parallel task scheduling

Small code module inspired by TheCerno's video on creating a C++ timer (https://www.youtube.com/watch?v=t11q4qgwngQ).

This code creates a `TaskScheduler` with a convenient interface, which may delegate tasks to main/parallel thread to
be executed after a specified time interval has passed. Intended for game engine programming, but can work under any
scenario which requires such a system.

