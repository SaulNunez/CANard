Project Architecture & Subsystem Guidelines

This document outlines the architectural patterns, directory organization, and inter-subsystem communication rules for this ESP-IDF project. Adhering to these conventions keeps the codebase modular, testable, and maintainable.
1. Project Layout & Component Isolation

We enforce strict separation between application orchestration and functional subsystems using ESP-IDF's native component system.

```
.
├── CMakeLists.txt              # Top-level project CMake configuration
├── main/                       # Application Orchestration
│   ├── CMakeLists.txt          # Main component registration
│   └── main.c                  # App entry point (app_main)
└── components/                 # Isolated Subsystems (Custom Components)
    ├── wifi_manager/           # Example: Network Handler Subsystem
    │   ├── CMakeLists.txt      # Component-level CMake configuration
    │   ├── include/            # Public API (Visible to other components)
    │   │   └── wifi_manager.h  
    │   └── wifi_manager.c      # Private implementation details
    └── sensors/                # Example: Data Acquisition Subsystem
        ├── CMakeLists.txt
        ├── include/
        │   └── sensor_events.h # Shared structures and event declarations
        └── sensors.c
```

Component Rules

    The main/ directory should only handle initial system initialization (NVS, default loops, driver baselines) and spawn individual subsystem tasks. It does not contain low-level driver or domain logic.

    The components/ directory houses isolated modules. Each folder inside components/ is treated as a standalone static library by the CMake build system.

2. Dependency Management (CMake)

To prevent circular dependencies and long compilation times, explicit component requirements must be declared in each component's local CMakeLists.txt.
For Subsystems (components/your_subsystem/CMakeLists.txt)

Explicitly declare source files, public include directories, and external framework components (like driver, nvs_flash, etc.):
CMake

idf_component_register(SRCS "your_subsystem.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver) # External dependencies

For the Application Layer (main/CMakeLists.txt)

Explicitly register your application's direct internal dependencies using PRIV_REQUIRES:
CMake

idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES wifi_manager sensors)

3. Decoupled Communication via esp_event

To maintain clean architectural boundaries, subsystems must never call each other's APIs directly. Tight coupling makes code brittle and prevents modules from being easily swapped or unit-tested.

Instead, all cross-component communication must leverage the ESP-IDF Default Event Loop.
Architectural Flow
Plaintext

[Subsystem Producer] ──(esp_event_post)──> [Default Event Loop] ──(Callback)──> [Subsystem Consumer]

Implementation Checklist

    Define Event Bases Separately: Declare custom event bases and payload data structures in a public header file inside the producing component (e.g., components/sensors/include/sensor_events.h).

    Export the Base Macro: Use ESP_EVENT_DECLARE_BASE() in the header file, and define it using ESP_EVENT_DEFINE_BASE() inside the corresponding .c file to allow the linker to resolve it globally.

    Thread-Safe Posting: Producers should use esp_event_post() to push data onto the system queue. The event loop takes care of deep-copying payloads, making data sharing memory-safe across tasks.

    Asynchronous Consumption: Consumers subscribe to target events using esp_event_handler_instance_register(). Event callbacks run in the context of the internal system event task (sys_evt), completely isolated from the producer task’s stack.

    Note: Because event execution occurs on the system event task, ensure that callback functions are highly performant and non-blocking. If a consumer needs to perform long-running I/O operation (e.g., flash writes or network requests), it should copy the payload into an independent FreeRTOS queue or trigger a dedicated processing worker task.