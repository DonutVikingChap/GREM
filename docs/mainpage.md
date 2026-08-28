# API Reference for GREM

GREM is a modular, cross-platform C++ library for building games, game engines and interactive applications.

@tableofcontents{HTML:1}

## Getting started

The source code for GREM can be found on [GitHub](https://github.com/DonutVikingChap/GREM). See the included `README.md` file for instructions on how to configure and build a new application project.

## API organization

The API of GREM is organized into 11 main modules and one core module, with the following dependencies between them:

@dotfile module_hierarchy.dot

Each main module is optional and can be disabled through CMake options as described in the project setup instructions, provided that they aren't required by another enabled module. Doing this can help improve compile times for applications that don't need e.g. physics or networking.

## Examples

GREM provides example code in the `examples/` directory, included with the repository, that shows how various features of the library work and are intended to be used. The examples are listed here, ordered by increasing complexity, along with the added functionality they demonstrate over their previously listed examples.

### examples/rectangle

This example is ~80 lines long and implements a basic application that renders a rectangle.

<details>
<summary>Features</summary>

| Functionality demonstrated                            | Module used   | Relevant components used |
| ----------------------------------------------------- | ------------- | ------------------------ |
| Basic application structure                           | `application` | [app::Application](@ref grem::application::Application) |
| Window and swapchain setup                            | `graphics`    | [gfx::Window](@ref grem::graphics::Window), [gfx::Device](@ref grem::graphics::Device), [gfx::Swapchain](@ref grem::graphics::Swapchain) |
| Basic event handling and window resizing              | `events`      | [evt::Eventpump](@ref grem::events::EventPump) |
| Basic 2D rendering setup                              | `graphics_2d` | [gfx::Renderer2D](@ref grem::graphics::Renderer2D), [gfx::Instances2D](@ref grem::graphics::Instances2D), [gfx::Camera2D](@ref grem::graphics::Camera2D), [gfx::Viewport](@ref grem::graphics::Viewport), [gfx::RenderPass](@ref grem::graphics::RenderPass) |

</details>

### examples/physics

This example is ~500 lines long and implements a basic 2D physics toy.

<details>
<summary>Added features</summary>

| Functionality demonstrated                                                           | Module used   | Relevant components used |
| ------------------------------------------------------------------------------------ | ------------- | ------------------------ |
| Command line options parsing                                                         | `core`        | [cli](@ref grem::cli) |
| Random number generation                                                             | `core`        | [rng::DefaultRandomEngine](@ref grem::randomness::DefaultRandomEngine), [rng::UniformIntegerDistribution](@ref grem::randomness::UniformIntegerDistribution), [rng::UniformRealDistribution](@ref grem::randomness::UniformRealDistribution) |
| Virtual filesystem setup                                                             | `application` | [app::VirtualFilesystem](@ref grem::application::VirtualFilesystem) |
| Full application structure                                                           | `application` | [app::Application](@ref grem::application::Application) |
| Frame rate-independent simulation (separating `tick()` from `update()`)              | `application` | [app::Application](@ref grem::application::Application) |
| High refresh rate support (interpolating between ticks in `display()` using `mix()`) | `application` | [app::Application](@ref grem::application::Application) |
| Message boxes (catching and showing fatal errors to the user)                        | `events`      | [evt::SimpleMessageBox](@ref grem::events::SimpleMessageBox) |
| Direct input event handling (sub-frame mouse movement and key presses)               | `events`      | [evt::EventPump](@ref grem::events::EventPump) |
| Multithreaded executor setup                                                         | `execution`   | [exec::DynamicExecutor](@ref grem::execution::DynamicExecutor) |
| Physics simulation setup                                                             | `physics`     | [phys::Simulation](@ref grem::physics::Simulation) |
| Physics object/joint creation                                                        | `physics`     | [phys::Simulation](@ref grem::physics::Simulation), [phys::EntityID](@ref grem::physics::EntityID) |
| Image loading                                                                        | `resource`    | [res::Image](@ref grem::resource::Image) |
| Font loading and text rendering                                                      | `graphics_2d` | [gfx::Instances2D](@ref grem::graphics::Instances2D), [gfx::Font2D](@ref grem::graphics::Font2D), [gfx::Text2D](@ref grem::graphics::Text2D) |
| Textured rectangle rendering                                                         | `graphics_2d` | [gfx::Instances2D](@ref grem::graphics::Instances2D), [gfx::Texture](@ref grem::graphics::Texture) |

</details>

### examples/test_game

This example game is ~700 lines long and is used to test some basic features of the library.

<details>
<summary>Added features</summary>

| Functionality demonstrated                                                                            | Module used   | Relevant components used |
| ----------------------------------------------------------------------------------------------------- | ------------- | ------------------------ |
| Configuration loading from JSON files                                                                 | `core`        | [json](@ref grem::json) |
| Shape intersection testing (basic, without using `physics`)                                           | `core`        | [LooseQuadtree](@ref grem::LooseQuadtree), [Box](@ref grem::Box), [Rectangle](@ref grem::Rectangle), [Circle](@ref grem::Circle), [Capsule](@ref grem::Capsule) |
| Input manager setup                                                                                   | `events`      | [evt::InputManager](@ref grem::events::InputManager) |
| User input bindings configuration                                                                     | `events`      | [evt::InputManager](@ref grem::events::InputManager) |
| Model loading                                                                                         | `resource`    | [res::Model](@ref grem::resource::Model) |
| Sound loading                                                                                         | `audio`       | [aud::Sound](@ref grem::audio::Sound) |
| Playing background music                                                                              | `audio`       | [aud::SoundStage](@ref grem::audio::SoundStage) |
| Custom shader code (see the `examples/data/test_game/shaders/` directory)                             | `graphics`    | [gfx::ShaderPipeline](@ref grem::graphics::ShaderPipeline), [gfx::FragmentShader](@ref grem::graphics::FragmentShader), [gfx::UniformBuffer](@ref grem::graphics::UniformBuffer) |
| Offline shader compilation for Vulkan (see `examples/test_game/shader_compiler.cpp` for instructions) | `graphics`    | [gfx::FragmentShader](@ref grem::graphics::FragmentShader) |
| Sprite rendering                                                                                      | `graphics_2d` | [gfx::Instances2D](@ref grem::graphics::Instances2D), [gfx::SpriteAtlas](@ref grem::graphics::SpriteAtlas), [gfx::SpriteID](@ref grem::graphics::SpriteID) |
| Basic 3D PBR rendering setup                                                                          | `graphics_3d` | [gfx::Renderer3D](@ref grem::graphics::Renderer3D), [gfx::Instances3D](@ref grem::graphics::Instances3D), [gfx::Model3D](@ref grem::graphics::Model3D), [gfx::Fog3D](@ref grem::graphics::Fog3D), [gfx::Sky3D](@ref grem::graphics::Sky3D), [gfx::Decals3D](@ref grem::graphics::Decals3D), [gfx::Lights3D](@ref grem::graphics::Lights3D), [gfx::LightProbeVolumes3D](@ref grem::graphics::LightProbeVolumes3D), [gfx::ReflectionProbes3D](@ref grem::graphics::ReflectionProbes3D), [gfx::Camera3D](@ref grem::graphics::Camera3D), [gfx::Viewport](@ref grem::graphics::Viewport), [gfx::RenderPass](@ref grem::graphics::RenderPass) |
| Dynamic lights                                                                                        | `graphics_3d` | [gfx::Lights3D](@ref grem::graphics::Lights3D) |
| In-game debug GUI setup                                                                               | `imgui`       | [imgui::GraphicalUserInterface](@ref grem::imgui::GraphicalUserInterface) |
| Showing the ImGui demo window                                                                         | `imgui`       | [imgui::GraphicalUserInterface](@ref grem::imgui::GraphicalUserInterface) |

</details>

### examples/tiles

This example is ~3500 lines split across 19 files and implements a tile-based 2D game.

<details>
<summary>Added features</summary>

| Functionality demonstrated                        | Relevant files                                               | Module used   | Relevant components used |
| ------------------------------------------------- | ------------------------------------------------------------ | ------------- | ------------------------ |
| String formatting of custom types                 | `%Coordinate.hpp`                                            | `core`        | [Formatter](@ref grem::Formatter) |
| Custom fixed-point coordinates                    | `%Coordinate.hpp`                                            | `core`        | [vec](@ref grem::vec) |
| Manual circle-vs-rectangle collision resolution   | `%Contacts.hpp`                                              | `core`        | [vec](@ref grem::vec), [Box](@ref grem::Box) |
| Custom JSON format reading                        | `%Map.hpp`, `%Schema.hpp`                                    | `core`        | [json](@ref grem::json) |
| Layer stack and menu navigation                   | `%Layer.hpp`, `%layers/MenuLayer.hpp`, `%main.cpp`           | `core`        | [ArrayList](@ref grem::ArrayList), [UniquePointer](@ref grem::UniquePointer), [Variant](@ref grem::Variant) |
| Converting mouse coordinates to world coordinates | `%Graphics.hpp`, `%layers/PlayLayer.hpp`                     | `events`      | [evt::EventPump](@ref grem::events::EventPump) |
| Entity Component System (ECS) game architecture   | `%World.hpp`                                                 | `execution`   | [exec::EntityRegistry](@ref grem::execution::EntityRegistry), [exec::ResourceRegistry](@ref grem::execution::ResourceRegistry), [exec::Entities](@ref grem::execution::Entities), [exec::EntityID](@ref grem::execution::EntityID) |
| Task scheduling with automatic parallelization    | `%World.hpp`                                                 | `execution`   | [exec::EntityRegistry](@ref grem::execution::EntityRegistry), [exec::ResourceRegistry](@ref grem::execution::ResourceRegistry), [exec::Entities](@ref grem::execution::Entities), [exec::EntityID](@ref grem::execution::EntityID), [exec::Schedule](@ref grem::execution::Schedule), [exec::Scheduler](@ref grem::execution::Scheduler), [exec::Executor](@ref grem::execution::Executor) |
| Advanced custom shaders                           | `%WorldRenderer.hpp`, `%shaders.hpp`                         | `graphics`    | [gfx::Mesh](@ref grem::graphics::Mesh), [gfx::ShaderPipeline](@ref grem::graphics::ShaderPipeline), [gfx::VertexShader](@ref grem::graphics::VertexShader), [gfx::FragmentShader](@ref grem::graphics::FragmentShader), [gfx::UniformBuffer](@ref grem::graphics::UniformBuffer) |

</details>

### examples/fps

This example is ~21000 lines split across 81 files and implements a basic FPS game prototype with online multiplayer support.

This project is significantly more advanced than the other examples, and is meant to represent a more realistic integration of GREM in a production-quality engine. For example, it includes GREM headers on a per-file basis to improve compile times, and has a modular archtiecture that allows individual game systems to be compiled separately and loaded like plugins at runtime.

<details>
<summary>Added features</summary>

| Functionality demonstrated                                                                     | Relevant files                                                              | Module used   | Relevant components used |
| ---------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- | ------------- | ------------------------ |
| Dynamically loaded shared libraries                                                            | `%GameSystems.cpp`                                                          | `core`        | [SharedLibrary](@ref grem::SharedLibrary) |
| Manual threading and synchronization                                                           | `%Game.hpp`                                                                 | `core`        | [Thread](@ref grem::Thread), [Mutex](@ref grem::Mutex), [ScopedLock](@ref grem::ScopedLock), [Atomic](@ref grem::Atomic), [AtomicFlag](@ref grem::AtomicFlag) |
| Persistent user settings with associated console commands                                      | `%ClientSettings.hpp`, `%GameClient.hpp`, `%GameClient.cpp`, `%Game.hpp`    | `core`        | [Filesystem](@ref grem::Filesystem), [meta](@ref grem::meta) |
| Asset caching with parallel loading                                                            | `%AssetCache.hpp`                                                           | `core`        | [Filesystem](@ref grem::Filesystem), [HashMap](@ref grem::HashMap), [Mutex](@ref grem::Mutex), [ScopedLock](@ref grem::ScopedLock), [AtomicFlag](@ref grem::AtomicFlag) |
| Custom binary data serialization protocol integration                                          | `%serialization.hpp`                                                        | `core`        | [Reader](@ref grem::Reader), [SpanReader](@ref grem::SpanReader), [Writer](@ref grem::Writer) |
| UDP networking with a custom connection protocol                                               | `%Connection.hpp`, `%GameServer.cpp`, `%GameClient.cpp`                     | `networking`  | [net::UDPSocket](@ref grem::networking::UDPSocket), [net::Endpoint](@ref grem::networking::Endpoint) |
| Advanced 3D physics simulation (using the static API with manual registry/schedule management) | `%PhysicsSimulationSystem.cpp`                                              | `physics`     | [phys::Simulation](@ref grem::physics::Simulation), [phys::Broadphase](@ref grem::physics::Broadphase), [phys::Contacts](@ref grem::physics::Contacts) |
| Physical units and quantities in game code                                                     | `%MovementControlSystem.cpp`, etc.                                          | `physics`     | [phys::Quantity](@ref grem::physics::Quantity) |
| Positional audio and advanced sound instance management                                        | `%SoundAudioStagingSystem.cpp`                                              | `audio`       | [aud::SoundStage](@ref grem::audio::SoundStage), [aud::Listener](@ref grem::audio::Listener) |
| Audio statistics visualization                                                                 | `%ClientStatisticsGraphicsStagingSystem.cpp`, `%GameClient.cpp`             | `audio`       | [aud::SoundStage](@ref grem::audio::SoundStage) |
| Sub-frame input manager output event handling                                                  | `%GameClient.cpp`                                                           | `events`      | [evt::EventPump](@ref grem::events::EventPump), [evt::InputManager](@ref grem::events::InputManager) |
| Splitscreen input manager setup                                                                | `%GameClient.cpp`                                                           | `events`      | [evt::InputManager](@ref grem::events::InputManager) |
| Splitscreen rendering                                                                          | `%Graphics.hpp`, `%WorldViewGraphicsRenderingSystem.cpp`, `%GameClient.cpp` | `graphics`    | [gfx::Texture](@ref grem::graphics::Texture), [gfx::Viewport](@ref grem::graphics::Viewport), [gfx::RenderPass](@ref grem::graphics::RenderPass) |
| Manual instance batching                                                                       | `%Graphics.hpp`, `%ModelGraphicsStagingSystem.cpp`                          | `graphics_3d` | [gfx::Instances3D](@ref grem::graphics::Instances3D) |
| Sky, fog and decal rendering                                                                   | `%Graphics.hpp`, `%DecalGraphicsStagingSystem.cpp`, `%GameClient.cpp`       | `graphics_3d` | [gfx::Sky3D](@ref grem::graphics::Sky3D), [gfx::Fog3D](@ref grem::graphics::Fog3D), [gfx::Decals3D](@ref grem::graphics::Decals3D) |
| Light baking and global illumination                                                           | `%Graphics.hpp`, `%LightBakingSystem.cpp`, `%GameClient.cpp`                | `graphics_3d` | [gfx::Sky3D](@ref grem::graphics::Sky3D), [gfx::LightProbeVolumes3D](@ref grem::graphics::LightProbeVolumes3D), [gfx::ReflectionProbes3D](@ref grem::graphics::ReflectionProbes3D), [gfx::LightBaker3D](@ref grem::graphics::LightBaker3D) |
| In-game debug GUI widgets, chat and developer console                                          | `%Game.hpp`, `%GameClient.cpp`, `%Console.hpp`                              | `imgui`       | [imgui::GraphicalUserInterface](@ref grem::imgui::GraphicalUserInterface) |

</details>

## Modules

Here is a list of all modules and their most important components:

- [grem::application](@ref grem::application) - Application shell
    - [Application](@ref grem::application::Application) - Main application base class
    - [VirtualFilesystem](@ref grem::application::VirtualFilesystem) - Filesystem implementation supporting mounting of input directories/archives and an optional output directory
- [grem::audio](@ref grem::audio) - Audio engine
    - [Sound](@ref grem::audio::Sound) - Sound wave loading
	- [SoundMix](@ref grem::audio::SoundMix) - Mixing bus of multiple sounds
    - [SoundStage](@ref grem::audio::SoundStage) - System for 3D sound playback to the default audio device
- [grem::events](@ref grem::events) - Events system
    - [EventPump](@ref grem::events::EventPump) - On-demand polling of events and user input from the host environment
    - [InputManager](@ref grem::events::InputManager) - Mapping between physical inputs and abstract output numbers
- [grem::execution](@ref grem::execution) - ECS-based task scheduling and multithreading
    - [EntityRegistry](@ref grem::execution::EntityRegistry) - Container of [EntityID](@ref grem::execution::EntityID)-mapped components available to scheduled tasks
	- [EntityTable](@ref grem::execution::EntityTable) - Alternative to EntityRegistry that uses indexed rows instead of entity IDs and a fixed number of type columns instead of a dynamic set of component pools
    - [Executor](@ref grem::execution::Executor) - Generic interface to a pool of execution resources (e.g. threads) for executing scheduled tasks
	- [SequentialExecutor](@ref grem::execution::SequentialExecutor) - Single-threaded executor implementation
	- [DynamicExecutor](@ref grem::execution::DynamicExecutor) - Dynamic executor whose implementation is chosen at runtime, defaulting to a parallel thread pool on supported platforms
    - [ResourceRegistry](@ref grem::execution::ResourceRegistry) - Container of shared resources available to scheduled tasks
    - [ResourceTable](@ref grem::execution::ResourceTable) - Alternative to ResourceRegistry that always holds a statically known set of resource types
    - [Schedule](@ref grem::execution::Schedule) - Compiled execution graph of tasks
    - [Scheduler](@ref grem::execution::Scheduler) - Execution graph builder
    - [Task](@ref grem::execution::Task) - Basic task function wrapper
- [grem::graphics](@ref grem::graphics) - Portable graphics hardware interface
    - [Device](@ref grem::graphics::Device) - Rendering context for a [Window](@ref grem::graphics::Window)
    - [Display](@ref grem::graphics::Display) - Information about a connected display
    - [RenderPass](@ref grem::graphics::RenderPass) - List of draw commands to be rendered to a set of render targets
    - [ShaderPipeline](@ref grem::graphics::ShaderPipeline) - Compiled pipeline of vertex and fragment shaders
    - [SpriteAtlas](@ref grem::graphics::SpriteAtlas) - Packing of images into an expandable spritesheet
	- [Swapchain](@ref grem::graphics::Swapchain) - Render target texture holding a chain of images to be presented to a [Window](@ref grem::graphics::Window)
    - [Texture](@ref grem::graphics::Texture) - Image data for a texture stored on the GPU
    - [Viewport](@ref grem::graphics::Viewport) - Region of a render target
    - [Window](@ref grem::graphics::Window) - Graphical window that can be rendered to
- [grem::graphics_2d](@ref include/GREM/graphics_2d.hpp) - Baseline 2D graphics rendering
    - [Camera2D](@ref grem::graphics::Camera2D) - Perspective to render from in 2D
    - [Font2D](@ref grem::graphics::Font2D) - Font loading for text rendering
    - [Instances2D](@ref grem::graphics::Instances2D) - Batch of 2D instances to be drawn to a [RenderPass](@ref grem::graphics::RenderPass)
    - [Model2D](@ref grem::graphics::Model2D) - Vertex/uniform data for a 2D model stored on the GPU
    - [Renderer2D](@ref grem::graphics::Renderer2D) - Renderer for 2D content
    - [Text2D](@ref grem::graphics::Text2D) - Text shaping facility
- [grem::graphics_3d](@ref include/GREM/graphics_3d.hpp) - Baseline 3D graphics rendering
    - [Camera3D](@ref grem::graphics::Camera3D) - Perspective to render from in 3D
    - [Instances3D](@ref grem::graphics::Instances3D) - Batch of 3D instances to be drawn to a [RenderPass](@ref grem::graphics::RenderPass)
    - [Model3D](@ref grem::graphics::Model3D) - Vertex/uniform data for a 3D model stored on the GPU
    - [Renderer3D](@ref grem::graphics::Renderer3D) - Renderer for 3D content
- [grem::imgui](@ref grem::imgui) - ImGui integration
	- [GraphicalUserInterface](@ref grem::imgui::GraphicalUserInterface) - Platform+renderer backend for Dear ImGui
- [grem::networking](@ref grem::networking) - Network communication
    - [Endpoint](@ref grem::networking::Endpoint) - IP address and port number of a network location
    - [Socket](@ref grem::networking::Socket) - Generic interface for sending/receiving data to/from an endpoint
    - [TCPListener](@ref grem::networking::TCPListener) - Broker for incoming connections over TCP
    - [TCPSocket](@ref grem::networking::TCPSocket) - Channel for sending/receiving a stream of data to/from a connected endpoint over TCP
    - [UDPSocket](@ref grem::networking::UDPSocket) - Channel for sending/receiving packets to/from arbitrary endpoints over UDP
- [grem::physics](@ref grem::physics) - Physics engine
    - [Simulation](@ref grem::physics::Simulation) - Discrete-time rigid body dynamics simulation
	- [quantities](@ref include/GREM/physics/quantities.hpp) - Physical quantity types with concrete units, such as [Position](@ref grem::physics::Position) and [Speed](@ref grem::physics::Speed)
    - [Shape](@ref grem::physics::Shape) - Shape of a physical object
		- [PointShape](@ref grem::physics::PointShape) - Infinitesimally small point shape
		- [LineSegmentShape](@ref grem::physics::LineSegmentShape) - Infinitesimally thin line segment shape
		- [InfiniteLineShape](@ref grem::physics::InfiniteLineShape) - Infinitesimally thin line shape of infinite length
		- [InfiniteHalfSpaceShape](@ref grem::physics::InfiniteHalfSpaceShape) - One-sided plane shape of infinite width that completely fills all space on the other side
		- [InfinitePlaneShape3D](@ref grem::physics::InfinitePlaneShape3D) - Two-sided plane shape of infinite width (only available in 3D, use InfiniteLineShape instead in 2D)
		- [BoxShape](@ref grem::physics::BoxShape) - Solid rectangular cuboid shape (AKA [RectangleShape2D](@ref grem::physics::RectangleShape2D))
		- [CubeShape](@ref grem::physics::CubeShape) - Box shape where all sides have equal length (AKA [SquareShape2D](@ref grem::physics::SquareShape2D))
		- [EllipsoidShape](@ref grem::physics::EllipsoidShape) - Solid ellipsoid shape (AKA [EllipseShape2D](@ref grem::physics::EllipseShape2D))
		- [SphereShape](@ref grem::physics::SphereShape) - Solid ball shape (AKA [CircleShape2D](@ref grem::physics::CircleShape2D))
		- [CapsuleShape](@ref grem::physics::CapsuleShape) - Solid pill shape (AKA [StadiumShape2D](@ref grem::physics::StadiumShape2D))
		- [TaperedCapsuleShape](@ref grem::physics::TaperedCapsuleShape) - Capsule shape with different top/bottom radii
		- [CylinderShape3D](@ref grem::physics::CylinderShape3D) - Solid cylinder shape (only available in 3D, use RectangleShape2D instead in 2D)
		- [TaperedCylinderShape3D](@ref grem::physics::TaperedCylinderShape3D) - Conical frustum shape with different top/bottom radii
		- [ConvexPolytopeShape](@ref grem::physics::ConvexPolytopeShape) - Solid convex hull of an arbitrary set of vertices
		- [TriangleMeshShape](@ref grem::physics::TriangleMeshShape) - Non-solid triangle soup shape (intended for static objects, does not collide with other triangle meshes)
		- [LocallyTransformedShape](@ref grem::physics::LocallyTransformedShape) - Wrapper of another shape with a local transformation applied to it (shifting its center of mass)
		- [CompoundColliderShape](@ref grem::physics::CompoundColliderShape) - Combined shape made from a set of colliders
- [grem::resource](@ref grem::resource) - Resource loading
    - [ArrayAtlasPacker](@ref grem::resource::ArrayAtlasPacker) - Rectangle packer for expandable square texture atlases with array layers
    - [AtlasPacker](@ref grem::resource::AtlasPacker) - Rectangle packer for expandable square texture atlases
    - [Image](@ref grem::resource::Image) - Loading/saving of images
    - [Model](@ref grem::resource::Model) - Loading of 3D models

### Core module

The core module is shared across all main modules and provides the following utility APIs under the `grem` namespace:

- `GREM/core/*` - Core utilities:
	- [algorithms](@ref include/GREM/core/algorithms.hpp) - Range-based wrappers of standard library algorithms
	- [assertions](@ref include/GREM/core/assertions.hpp) - Provides GREM_ASSERT() and grem::unreachable()
	- [attributes](@ref include/GREM/core/attributes.hpp) - Portable aliases of non-standard declaration attributes that work on most compilers
	- [command_line_interface](@ref grem::cli) - Command-line argument/option parsing
	- [concepts](@ref include/GREM/core/concepts.hpp) - Collection of basic C++20 concepts
	- [control](@ref include/GREM/core/control.hpp) - Basic control engineering utilities such as [PIDController](@ref grem::PIDController)
	- [Error](@ref grem::Error) - Base exception type for runtime errors
	- [extents](@ref include/GREM/core/extents.hpp) - Integer coordinate types for specifying regions of multidimensional data (such as images)
	- [formatting](@ref include/GREM/core/formatting.hpp) - String formatting and printing utilities (replacement for std::format/std::print)
	- [fundamentals](@ref include/GREM/core/fundamentals.hpp) - Definitions of primitive fixed-width boolean, integer and floating-point types and basic operations on them
	- [geometry](@ref include/GREM/core/geometry.hpp) - Basic geometric shapes
	- [math](@ref include/GREM/core/math.hpp) - Mathematical functions and primitives such as vector, matrix and quaternion types
    - [metaprogramming](@ref grem::meta) - Compile-time metaprogramming utilities, including reflection of aggregate types
	- [profiling](@ref include/GREM/core/profiling.hpp) - Performance profiling framework, enabled with the GREM_USE_PROFILING CMake option
    - [randomness](@ref grem::randomness) - Pseudo-random number generation (replacement for std::mt19937 and aliases of std::uniform_int_distribution, etc.)
	- [statistics](@ref include/GREM/core/statistics.hpp) - Basic utilities for statistics such as [ExponentialMovingAverage](@ref grem::ExponentialMovingAverage) and [SlidingWindow](@ref grem::SlidingWindow)
	- [time](@ref include/GREM/core/time.hpp) - Duration and time point types and utilities (aliasing std::chrono types)
	- [version](@ref include/GREM/core/version.hpp) - Provides ABI-stable functions for querying the loaded library version at runtime
- `GREM/core/data/*` - Data types:
    - [Allocation](@ref grem::Allocation) - Dynamically allocated non-growable array
    - [Any](@ref grem::Any) - Small-object-optimized type-erased wrapper for regular value types (replacement for std::any)
    - [Array](@ref grem::Array) - Zero-allocation fixed-size array (replacement for std::array)
    - [ArrayList](@ref grem::ArrayList) - Dynamically allocated growable array (replacement for std::vector)
    - [BitArray](@ref grem::BitArray) - Zero-allocation fixed-size packed array of bits (replacement for std::bitset)
    - [BitBuffer](@ref grem::BitBuffer) - Dynamically allocated growable packed array of bits (replacement for std::vector&lt;bool&gt;)
    - [Buffer](@ref grem::Buffer) - Dynamically allocated growable array that preserves erased elements for nested memory reuse (and may use malloc/realloc instead of std::allocator for trivial types)
    - [Color](@ref grem::Color) - Floating-point linear RGBA color type
    - [CStringView](@ref grem::CStringView) - Read-only view over a null-terminated string (replacement for const char\*)
    - [DoubleEndedQueue](@ref grem::DoubleEndedQueue) - Ring buffer-based double-ended queue (replacement for std::deque)
    - [DoublyLinkedList](@ref grem::DoublyLinkedList) - Doubly-linked list container (alias of std::list)
    - [Function](@ref grem::Function) - Type-erased value wrapper for a callable function object (alias of std::function)
    - [FunctionView](@ref grem::FunctionView) - Type-erased proxy for a callable function object
    - [HashMap](@ref grem::HashMap) - Linear probing hash table that maps unique keys to values (replacement for std::unordered_map)
    - [HashSet](@ref grem::HashSet) - Linear probing hash table containing a set of unique keys (replacement for std::unordered_set)
    - [OrderedMap](@ref grem::OrderedMap) - Ordered associative array mapping unique keys to values (replacement for std::map)
    - [OrderedMultimap](@ref grem::OrderedMultimap) - Ordered associative array mapping keys to any number of values (replacement for std::multimap)
    - [Indirect](@ref grem::Indirect) - Allocated value wrapper (replacement for std::indirect)
    - [InplaceArrayList](@ref grem::InplaceArrayList) - Zero-allocation growable array with fixed capacity
    - [InplaceBuffer](@ref grem::InplaceBuffer) - Zero-allocation growable array with fixed capacity that preserves erased elements for nested memory reuse
    - [InplaceDoubleEndedQueue](@ref grem::InplaceDoubleEndedQueue) - Zero-allocation double-ended queue with fixed capacity
    - [InplaceRingBuffer](@ref grem::InplaceRingBuffer) - Zero-allocation double-ended queue with fixed capacity that preserves erased elements for nested memory reuse
    - [LinearBuffer](@ref grem::LinearBuffer) - Append-only buffer of trivially copyable types
    - [LooseOrthtree](@ref grem::LooseOrthtree) - Space subdivision structure for fast intersection tests against a large number of AABBs
    - [MPMCQueue](@ref grem::MPMCQueue) - Multi-producer multi-consumer queue
    - [Optional](@ref grem::Optional) - Optional value type (replacement for std::optional)
	- [Overloaded](@ref grem::Overloaded) - Utility for creating overload sets from lambdas
    - [Pair](@ref grem::Pair) - Pair of values (replacement for std::pair)
    - [PriorityQueue](@ref grem::PriorityQueue) - Priority queue container wrapper (alias of std::priority_queue)
    - [RangeAllocator](@ref grem::RangeAllocator) - Allocates contiguous integer ranges without associated memory
	- [Reader](@ref grem::Reader) - Type-erased interface for reading binary data.
    - [Registry](@ref grem::Registry) - Tightly packed set of elements with stable identifiers
    - [RingBuffer](@ref grem::RingBuffer) - Ring buffer-based double-ended queue that preserves erased elements for nested memory reuse
    - [SharedPointer](@ref grem::SharedPointer) - Data pointer with shared ownership (replacement for std::shared_ptr)
    - [SinglyLinkedList](@ref grem::SinglyLinkedList) - Singly-linked list container (alias of std::forward_list)
    - [SmallArrayList](@ref grem::SmallArrayList) - Small-buffer-optimized growable array
    - [SmallBuffer](@ref grem::SmallBuffer) - Small-buffer-optimized growable array that preserves erased elements for nested memory reuse
    - [Span](@ref grem::Span) - View over a contiguous sequence of values (replacement for std::span)
    - [SPSCQueue](@ref grem::SPSCQueue) - Single-producer single-consumer queue
    - [SPSCSlidingWindowQueue](@ref grem::SPSCSlidingWindowQueue) - Single-producer single-consumer queue with a sliding window of readable data
    - [StridedSpan](@ref grem::StridedSpan) - View over a sequence of values with an arbitrary constant stride between them
    - [String](@ref grem::String) - Small-buffer-optimized owning string of characters (alias of std::string)
    - [StringPool](@ref grem::StringPool) - Compact growable set of interned immutable strings with associated [StringID](@ref grem::StringID)s
    - [StringView](@ref grem::StringView) - Read-only view over a string of characters (alias of std::string_view)
    - [Table](@ref grem::Table) - Column-major two-dimensional structure-of-arrays container
    - [Tuple](@ref grem::Tuple) - Tuple of values (replacement for std::tuple)
    - [UniqueHandle](@ref grem::UniqueHandle) - Generic resource handle with exclusive ownership
    - [UniquePointer](@ref grem::UniquePointer) - Data pointer with exclusive ownership (replacement for std::unique_ptr)
    - [Variant](@ref grem::Variant) - Tagged union value type (replacement for std::variant)
	- [Writer](@ref grem::Writer) - Type-erased interface for writing binary data.
- `GREM/core/formats/*` - Data interchange formats:
	- [Adler32](@ref grem::Adler32) - Adler-32 checksum
    - [ascii](@ref grem::ascii) - ASCII character utilities
    - [base16](@ref grem::base16) - Base16 (hexadecimal) string encoding/decoding
    - [base64](@ref grem::base64) - Base64 string encoding/decoding
    - [CRC32](@ref grem::CRC32) - Cyclic redundancy check
    - [deflate](@ref grem::deflate) - Deflate compression/decompression
    - [gltf](@ref grem::gltf) - glTF asset parsing
    - [json](@ref grem::json) - JSON parsing/writing/(de)serialization
    - [obj](@ref grem::obj) - OBJ and MTL file parsing
    - [unicode](@ref grem::unicode) - UTF-8 text decoding
    - [uri](@ref grem::uri) - Percent encoding/decoding
    - [xml](@ref grem::xml) - Basic XML document parsing
- `GREM/core/system/*` - Host system I/O:
    - [Clock](@ref grem::Clock) - Monotonic clock for measuring time intervals with high precision (alias of std::chrono::steady_clock)
    - [Filesystem](@ref grem::Filesystem) - Generic filesystem interface
	- [InputFileHandle](@ref grem::InputFileHandle) - Boxed generic handle to any readable file
	- [OutputFileHandle](@ref grem::OutputFileHandle) - Boxed generic handle to any writable file
    - [NativeFilesystem](@ref grem::NativeFilesystem) - Filesystem implementation providing direct access to the host filesystem using its native APIs
	- [SharedLibrary](@ref grem::SharedLibrary) - Library of dynamically loaded symbols (on platforms that support it)
	- [Thread](@ref grem::Thread) - Concurrent thread of execution (wrapper of std::thread) (on platforms that support multithreading)
	- [synchronization](@ref include/GREM/core/system/synchronization.hpp) - Synchronization primitives (for platforms that support multithreading):
		- [Atomic](@ref grem::Atomic) - Atomic variable (wrapper of std::atomic)
		- [AtomicFlag](@ref grem::AtomicFlag) - Atomic boolean that is guaranteed to be lock-free (wrapper of std::atomic_flag)
		- [AtomicRef](@ref grem::AtomicRef) - Temporary atomic access to a variable (wrapper of std::atomic_ref, or reimplementation on standard library versions that don't include it)
		- [Mutex](@ref grem::Mutex) - Synchronization primitive for enforcing mutual exclusion (wrapper of std::mutex)
		- [RecursiveMutex](@ref grem::RecursiveMutex) - Mutex that supports recursive locking (wrapper of std::recursive_mutex)
		- [SharedMutex](@ref grem::SharedMutex ) - Mutex that supports shared locking (wrapper of std::shared_mutex)
		- [TimedMutex](@ref grem::TimedMutex) - Mutex that supports locking with a timeout (wrapper of std::timed_mutex)
		- [RecursiveTimedMutex](@ref grem::RecursiveTimedMutex) - Mutex that supports recursive locking with a timeout (wrapper of std::recursive_timed_mutex)
		- [SharedTimedMutex](@ref grem::SharedTimedMutex) - Mutex that supports shared locking with a timeout (wrapper of std::shared_timed_mutex)
		- [ScopedLock](@ref grem::ScopedLock) - Lock that holds exclusive access for a mutex during a specific scope (wrapper of std::scoped_lock)
		- [UniqueLock](@ref grem::UniqueLock) - Movable lock that holds exclusive access for a mutex (wrapper of std::unique_lock)
		- [SharedLock](@ref grem::SharedLock) - Movable lock that holds shared access for a shared mutex (wrapper of std::shared_lock)

## Includes

GREM provides the following interface header files which transitively include all headers from their respective modules:

```cpp
#include <GREM/application.hpp>
#include <GREM/audio.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>
#include <GREM/execution.hpp>
#include <GREM/graphics.hpp>
#include <GREM/imgui.hpp>
#include <GREM/networking.hpp>
#include <GREM/physics.hpp>
#include <GREM/resource.hpp>
```

Alternatively, all enabled modules can be included like this:

```cpp
#include <GREM/GREM.hpp>
```

### Aliases

In addition, for applications where the potential risk of naming conflicts is acceptable, the following include can be used to provide short, global aliases for the GREM API:

```cpp
#include <GREM/aliases.hpp>
```

This file declares `using namespace grem;`, promoting all names defined directly in the `grem` namespace to the global namespace, as well as the following shortened namespace aliases of its nested namespaces:

```cpp
namespace app = grem::application;
namespace aud = grem::audio;
namespace evt = grem::events;
namespace exec = grem::execution;
namespace gfx = grem::graphics;
namespace imgui = grem::imgui;
namespace net = grem::networking;
namespace phys = grem::physics;
namespace res = grem::resource;

namespace ascii = grem::ascii;
namespace base16 = grem::base16;
namespace base64 = grem::base64;
namespace cli = grem::cli;
namespace deflate = grem::deflate;
namespace json = grem::json;
namespace meta = grem::meta;
namespace numbers = grem::numbers;
namespace obj = grem::obj;
namespace rng = grem::randomness;
namespace unicode = grem::unicode;
namespace xml = grem::xml;
```

### Entry point header

There is also one additional header, `<GREM/entry_point.hpp>`, which is not part of any particular module.

The purpose of this header is to make sure that `WinMain()` gets properly defined on Win32 platforms, and excluded on others. To get this portability benefit, `<GREM/entry_point.hpp>` should be included in your application's main source file that contains the definition of the `main()` function, and must not be included in any other file. It works by defining `main()` and/or `WinMain()` to call `GREM_private_main()`, and uses a macro to make your definition of `main()` actually define `GREM_private_main()` instead. Note that, unlike what the standard specifies for `main()`, this requires your definition to always take `(int, char*[])` as parameters and always return an `int` exit code explicitly, even if they are unused.

### Config header

The header `<GREM/build_config.hpp>` is included in all other GREM headers and contains some global macro definitions used throughout the library for e.g. DLL support on Win32 platforms. This header does not need to be included manually in user code.

## Choosing a graphics backend

GREM currently provides two separate implementations of the `graphics` module's backend using different hardware APIs, **OpenGL** (3.3 Core, ES 3.0 or WebGL 2.0) and **Vulkan** (1.2), optimized for different use cases.

To specify which backend to use, set the `GREM_GRAPHICS_BACKEND` CMake option to either `OpenGL` or `Vulkan`. This defaults to `OpenGL` if not specified. The OpenGL backend also provides the option `GREM_GRAPHICS_OPENGL_USE_ES_PROFILE`, which enables the OpenGL ES profile (also used for WebGL). This defaults to `OFF`, but should be set to `ON` for web/mobile builds that don't support OpenGL Core. The included CMake preset for Emscripten already does this, but custom presets need to set it manually.

While both the OpenGL and Vulkan implementations support a common subset of features that enable the full `graphics_2d` and `graphics_3d` modules to work, they have some different pros and cons:

### Advantages of the OpenGL backend

- Required for web builds (translates to WebGL).
- Better compatibility with mobile GPUs released around 2013-2023 (ES profile) and desktop GPUs released around 2008-2012 (Core profile).
- Requires fewer build dependencies.
- Natively supports runtime shader compilation (built into the user's graphics driver, doesn't require the glslang dependency).
- Produces a slightly smaller program executable.
- Compiles faster on the initial build of the graphics module.

### Advantages of the Vulkan backend

- Lower CPU and GPU overhead when rendering many instances (enabling higher frame rates).
- Better compatibility with new GPUs and driver versions (potentially more future-proof).
- Supports pre-compiled SPIR-V shader code (runtime shader compilation can be disabled entirely, shader source code can be excluded from distribution).
- Natively supports cubemap array textures with seamless filtering (emulated with 2D array textures and sampling tricks on OpenGL, which may produce rendering artifacts in reflections, etc.).
- Supports more compressed texture formats (limited to BC1 (DXT1) and BC3 (DXT5) on OpenGL to facilitate flipping on upload).
- Supports `grem::graphics::Device::awaitPresentation()` for awaiting individual frames (when the `VK_KHR_present_id` and `VK_KHR_present_wait` extensions are available).
- Higher depth precision (NDC space is natively 0-1 on the Z axis, OpenGL requires a conversion).
- Better debugging tools available (validation layers, shader debugging in RenderDoc, etc.).

If you are unsure which backend best suits your specific application or target platform based on these points, the simplest option is to leave it on the OpenGL default and switch to Vulkan later if you encounter frame rate/quality issues. You can also try both to see which one yields the best performance/compatibility on different devices. In general, OpenGL works great for 2D games, while Vulkan is recommended for 3D games on platforms that support it.

## Writing custom shaders

The `graphics` module expects any custom shaders to be written in a subset of the [OpenGL Shading Language](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.pdf) (GLSL) compatible with Vulkan 1.2, OpenGL 3.3 Core and OpenGL ES 3.0. Most of the differences between these versions are handled automatically by the graphics module, which does so by generating the necessary boilerplate as a shell around the application-provided shader code, including input/output declarations and buffer layouts specified by the shader type.

However, there are some differences between the Vulkan and OpenGL backends that are only mitigated by requiring shaders to use non-standard syntax, namely:

- Textures must be sampled using `GREM_textureSample2D()`, `GREM_textureSampleCubeArrayShadow()`, `GREM_texelFetch2D()`, etc. instead of `texture()`/`texelFetch()`.
- The current vertex index must be read using `GREM_vertexIndex` instead of `gl_VertexID`.
- The current fragment coordinates must be read using `GREM_fragmentCoordinates` instead of `gl_FragCoord`.

Another important difference is that the OpenGL ES version of the language is more strict regarding type conversions than OpenGL Core, leading to code like `float x = 0;` breaking portability and therefore being considered invalid (though non-ES builds may not detect this!). This example should be written `float x = 0.0;` in order to be portable and correct, since GLSL considers `0.0` a `float` while `0` is an `int`. Similarly, variables/parameters of type `uint` must use syntax like `123u` instead of `123`, and use explicit conversions like `uint x = uint(y);` when `y` is of type `int`.

### Shader headers

Shaders support two kinds of `#include` directives: relative paths in double quotes, e.g. `#include "my_shader_defines.glsl"`, and built-in headers provided by GREM in angle brackets, e.g. `#include <GREM/tonemapping.glsl>`. The provided built-in headers are:

- `<GREM/quaternion.glsl>` - Provides utilities for working with quaternions.
- `<GREM/numbers.glsl>` - Defines `GREM_PI`.
- `<GREM/tonemapping.glsl>` - Provides `GREM_tonemap()` for tonemapping from HDR to LDR.
- `<GREM/gamma_correction.glsl>` - Provides utilities for converting color to/from sRGB.
- `<GREM/blending.glsl>` - Provides color compositing utilities.
- `<GREM/irradiance.glsl>` - Provides functions for encoding/decoding irradiance values to/from lower-bitdepth textures.
- `<GREM/light.glsl>` - Defines a PBR-compatible `GREM_Light` struct.
- `<GREM/pbr.glsl>` - Provides utilities for physically based rendering.
- `<GREM/sampling.glsl>` - Provides coordinate space translation utilities for sampling.
- `<GREM/material.glsl>` - Defines a PBR-compatible `GREM_Material` struct.
- `<GREM/Model3D/vertex.glsl>` - Provides getters for interpreting the vertex attributes of a `grem::graphics::Model3D` in a vertex shader.
- `<GREM/Model3D/fragment.glsl>` - Provides getters for interpreting the material of a `grem::graphics::Model3D` in a fragment shader.
- `<GREM/Fog3D/fragment.glsl>` - Provides functions for applying `grem::graphics::Fog3D` in a fragment shader.
- `<GREM/Sky3D/fragment.glsl>` - Provides functions for sampling a `grem::graphics::Sky3D` in a fragment shader.
- `<GREM/Decals3D/fragment.glsl>` - Provides functions for sampling a set of `grem::graphics::Decals3D` and applying it to a material in a fragment shader.
- `<GREM/Lights3D/fragment.glsl>` - Provides functions for sampling a set of `grem::graphics::Lights3D` in a fragment shader.
- `<GREM/LightProbeVolumes3D/fragment.glsl>` - Provides functions for sampling a set of `grem::graphics::LightProbeVolumes3D` in a fragment shader.
- `<GREM/ReflectionProbes3D/fragment.glsl>` - Provides functions for sampling a set of `grem::graphics::ReflectionProbes3D` in a fragment shader.

For the full definitions of these headers, see the top of `src/graphics/shaders.cpp`. For examples of their usage, see the built-in shaders like `shaders/graphics_3d/renderer_3d_model_3d_pbr.frag`.

Custom shader headers also support the `#pragma once` directive as a more efficient alternative to header guards.

### Shader compilation on Vulkan

Compiling custom GLSL shaders under the Vulkan backend requires the `GREM_GRAPHICS_VULKAN_USE_GLSL_COMPILATION` CMake option to be set to `ON`. This also requires [Python 3](https://www.python.org/) to be installed and available to CMake in order to successfully configure the SPIRV-Tools dependency.

When this option is enabled, shaders can be compiled on-the-fly on Vulkan just as they can on OpenGL, which is useful for prototyping and debugging. However, note that before distributing the application, it is recommended to pre-compile the shaders using a shader compiler program and then set this option back to `OFF`, as mentioned under **Dependencies** in `README.md`.

For more information on pre-compiling GLSL shaders to SPIR-V for Vulkan, see the comment in `examples/test_game/shader_compiler.cpp`.

## API conventions

Except where the documentation specifies otherwise, the API uses the following conventions by default, in addition to those implied by the C++ standard:

### Safety

- The documentation uses the word "must" to specify that failure to meet a condition yields undefined behavior.
- The following names identify implementation details that are not part of the public API and may be changed/removed in future versions (even patches), and must therefore not be used directly, despite potentially being accessible due to language limitations:
	- Any name in a nested `detail` namespace.
	- Any name starting with `GREM_PRIVATE_` or `GREM_private_`.
	- Any member name starting with `_private_`.
- Throwing functions provide the following exception safety guarantee by default: When an exception is thrown, mutable objects/arguments passed to the throwing function are left in a destructible but unspecified state with no resources leaked. In this state, the affected objects always support destruction, reassignment and `noexcept` functions that restore them to a known state without preconditions (like `clear()` and `reset()`), but other operations may yield undefined behavior.
- Member functions of types in the `core/data` subdirectory provide whichever exception guarantee can be inferred from their current implementation. Their exception safety may be strengthened in future API versions, but they will not be weakened unless the major version is incremented.
- Any function that is not marked `noexcept` may be extended to throw any exception type in a future API version of the library, without changing the ABI version.
- Pointers, views and references returned from a member function are tied to the lifetime of the object on which the function was called.
- Non-null pointers, views and references passed to a function, whether directly or as part of an object, must remain valid until the function returns.
- When multiple pointers, views or references are passed to a function, and at least one of them is writable, they may not alias each other or reference overlapping memory regions.
- Enumeration values passed to a function, whether directly or as part of an object, must be one of the valid named enumerands of the enum type, unless the enum type has no named enumerands (like `byte`).
- Functions that mutate an object/argument are not thread-safe, and require exclusive access to the object until the function returns.
- Functions declared in the `graphics`, `graphics_2d`, `graphics_3d` or `imgui` modules may only be called from the main thread of the program, i.e. the thread that invoked main(). This also includes member functions, overloaded operators, constructors and destructors from these modules, with the exception of aggregate types like `grem::graphics::ModelInstance3D` where all the fields are public and also do not fall under this restriction themselves.

### Units and coordinate spaces

- World coordinates and physical quantities are expressed in standard SI units (kilograms, meters, seconds, etc.).
- Graphics and linear algebra operations use OpenGL conventions, such as normalized lower-left-origin texture coordinates, a left-handed [-1, 1] NDC space and a right-handed world coordinate system with Y up, with the following exceptions:
	- The NDC depth (Z) coordinate range is [0, 1] instead of [-1, 1] (needed to support Vulkan, affects the implementation of `grem::ortho()`, `grem::perspective()`, etc.).
	- Window-relative fragment coordinates, provided by `grem::events::EventPump`, `grem::graphics::Window` or `grem::graphics::Display`, or through `GREM_fragmentCoordinates` in shaders, are relative to an upper left origin instead of a lower left origin (also to support Vulkan).
	- Images uploaded to textures are sampled using the same texture coordinate space as textures rendered through framebuffers are, instead of appearing to be flipped vertically.
- CPU-side image pixels are stored tightly packed in row-major order, starting at the top left, while GPU-side textures are sampled relative to the bottom left. On OpenGL, this causes a vertical flip to occur behind the scenes on image upload for consistency with framebuffer output, though not on Vulkan. The Vulkan backend stores textures in the same order as CPU images and automatically flips the sampling in the shader instead to achieve consistency.
- Tangent-space basis matrices use TBN column order (tangent, bitangent, normal).
- Shaders work in linear Rec. 709 color space by default and perform gamma correction to/from sRGB automatically based on texture format.
- Shaders expect color textures with an alpha channel to be stored with pre-multiplied alpha. The output color written by shaders is also in pre-multiplied form. Bitmap CPU images are assumed have straight alpha (like PNGs) and are automatically pre-multiplied when uploaded to a texture by default.

### External documentation

- API documentation is omitted for core data structures and interfaces that mimic or closely match corresponding interfaces in the standard library or other standard APIs.
- For C/C++ standard library documentation, the recommended resource is [cppreference.com](https://cppreference.com/).
- For GLSL documentation, the recommended resource is the [Khronos OpenGL and OpenGL ES reference pages](https://registry.khronos.org/OpenGL-Refpages/).

## Naming conventions

GREM uses the naming conventions listed below to reduce internal naming conflicts and provide semantic hints to the reader regarding the purpose of different kinds of entities declared by the API.

There is no requirement to follow these naming conventions in user code, nor any particular recommendation to use this style, but it is provided here for reference in case anyone finds it useful for reading/extending the API or as inspiration for their own code.

<details>
<summary>Casing</summary>

- **Functions and methods** use `camelCase`, e.g. `formatString()`.
- **Variables, fields and parameters** use `camelCase`, e.g. `deltaTime`.
- **Compile-time constants** use `UPPER_SNAKE_CASE`, e.g. `MAX_TILE_COUNT`.
- **Types, structs and classes** use `PascalCase`, e.g. `AtomicFlag`.
- **Non-type template parameters** use `PascalCase`, e.g. `SmallCapacity`.
- **Concepts and metafunctions** use `snake_case`, e.g. `equality_comparable`.
- **Macros and preprocessor definitions** use `UPPER_SNAKE_CASE`, e.g. `GREM_ALWAYS_INLINE`.
- **Namespaces and filenames** use `snake_case`, e.g. `command_line_interface.hpp`, unless they are associated with a specific named entity, e.g. `Window.hpp`.
- **Acronyms** do not affect the casing of adjacent words, and are always capitalized unless they are the first word in a normally lower-camelCased name. Valid examples include:
	- `SPSCQueue` (type "SPSC queue")
	- `fromSRGB()` (function "from sRGB")
	- `rgbCoefficients` (parameter "RGB coefficients")

</details>

<details>
<summary>Affixes</summary>

- **Macros and preprocessor definitions** are prefixed with the library name and the module they are declared in to reduce naming conflicts, e.g. `GREM_PHYSICS_USE_DEBUG_VISUALIZATION`.
- **Metafunction type aliases** use a `_t` suffix, e.g. `variant_alternative_t`.
- **Metafunction variable aliases** use a `_v` suffix, e.g. `variant_index_v`.
- **Read-only view types** with reference semantics use a `View` suffix, e.g. `ImageView`.
- **Writable view types** with reference semantics use a `Reference` suffix, e.g. `ImageReference`.
- **Non-abstract base classes** use a `Base` suffix, e.g. `EventBase`.
- **CRTP base class templates** use a `Base` suffix, e.g. `RegistryElementIDBase`.
- **Type templates with a default concrete alias** use a `Base` suffix, e.g. `StringBase`.
- **Non-allocating container variants** use an `Inplace` prefix, e.g. `InplaceArrayList`.
- **Small-buffer-optimized container variants** use a `Small` prefix, e.g. `SmallArrayList`.

</details>

<details>
<summary>Grammar</summary>

- **Types** use a _noun_ or _adjective-noun_ naming style (often with a noun as adjective), e.g. `Window`, `ConvexPolytope` or `EntityTable`.
- **Pluralized type names** indicate types that represent collections of things, e.g. `Contacts` or `Lights3D`.
- **Impure procedures and non-const methods** use a _verb-noun_ or _verb-adjective(s)-noun_ naming style by default, e.g. `handleEvent()` or `putShadedTextInstance()`. If the grammatical object is implied by a parameter or the type being acted upon, the noun part may be omitted, e.g. `serialize(stream, value)` or `pidController.reset()`.
- **Pure functions and const member functions**, except for common math functions like `sqrt()`, typically use one of the following prefixes:
	- `get` for getters that return a value or reference in (amortized) `O(1)` time, e.g. `getWidth()`.
	- `is` for checks that return a boolean in (amortized) `O(1)` time, or `O(n)` time for ranges, e.g. `isUppercase()`.
	- `calculate` or `build` for potentially expensive functions that produce a value, e.g. `calculateBoundingBox()` or `buildSchedule()`.
	- `find` for potentially expensive functions that look something up and return it if found, or an empty/null value otherwise, e.g. `findInputByIdentifier()`.
	- `convert` for functions that map a value to a logically corresponding value in a different format in `O(1)` time for scalars or `O(n)` time for ranges, e.g. `convertLinearToSRGB()`.
	- `to` for potentially expensive functions that map an object to a corresponding value of a different type, e.g. `toString()`.
	- `as` for functions that map an object to a reference or view of a different type in `O(1)` time, e.g. `asBytes()`.
	- `from` for static member functions that create an instance of the associated class by converting their function arguments, e.g. `Color::fromSRGB()`.
	- `format` for potentially expensive functions that build a string, e.g. `formatCurrentExceptionMessage()`.

</details>

<details>
<summary>General</summary>

- **Pointer and reference variables** are named as if they were the referenced/pointed-to value itself when the concrete pointer type is an implementation detail, e.g. `elements` (rather than `pElements`, `elementPointer` or `pointerToElements`).
- **Interfaces and abstract base classes** use the same naming convention as regular types, e.g. `Executor`.
- **Templates** use the same naming convention as the kind of entity they are a template of, e.g. `ArrayList`.
- **Aliases** use the same naming convention as the kind of entity they are an alias of, e.g. `LooseQuadtree`.
- Longer, more descriptive names are generally preferred over succinct names, e.g. `inversePrincipalMomentsOfInertia` over `invIDiag`.
- The following short names have specific meanings in most cases:
	- `x`, `y`, `z` and `w` are coordinates related to dimensional axes.
	- `r`, `g`, `b` and `a` are red, green, blue and alpha color components.
	- `a`, `b`, `c`, etc. are the positional arguments of an operator or math function.
	- `i` and `j` are short-lived indices, usually loop variables.
	- `it` is a short-lived or often-advancing iterator, usually a loop variable or the result of `find()`/`insert()`/`emplace()` on a container.
	- `p` is a short-lived or often-advancing pointer, usually a loop variable.
	- `ch` is a short-lived character, usually an element of a string.
	- `kv` is a short-lived key-value pair, usually an element of a map.
	- `output` is the main out-parameter of the current function.
	- `result` is the return value of the current function.

</details>

Note that the naming conventions are occasionally broken in order to match a standard or external API (for familiarity, concept conformance or consistency), such as:
- `uint32_t` (from the C standard library) instead of `UInt32`,
- `empty()` (from the C++ standard library) instead of `isEmpty()` or
- `vec2` and `sampler2D` (from GLSL) instead of `FloatVector2D` and `Sampler2D`.
