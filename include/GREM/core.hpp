// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_HPP
#define GREM_CORE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>                              // IWYU pragma: export
#include <GREM/core/algorithms.hpp>                         // IWYU pragma: export
#include <GREM/core/assertions.hpp>                         // IWYU pragma: export
#include <GREM/core/attributes.hpp>                         // IWYU pragma: export
#include <GREM/core/command_line_interface.hpp>             // IWYU pragma: export
#include <GREM/core/concepts.hpp>                           // IWYU pragma: export
#include <GREM/core/control.hpp>                            // IWYU pragma: export
#include <GREM/core/data/Allocation.hpp>                    // IWYU pragma: export
#include <GREM/core/data/Any.hpp>                           // IWYU pragma: export
#include <GREM/core/data/Arena.hpp>                         // IWYU pragma: export
#include <GREM/core/data/Array.hpp>                         // IWYU pragma: export
#include <GREM/core/data/ArrayList.hpp>                     // IWYU pragma: export
#include <GREM/core/data/BitArray.hpp>                      // IWYU pragma: export
#include <GREM/core/data/BitBuffer.hpp>                     // IWYU pragma: export
#include <GREM/core/data/Buffer.hpp>                        // IWYU pragma: export
#include <GREM/core/data/CStringView.hpp>                   // IWYU pragma: export
#include <GREM/core/data/Color.hpp>                         // IWYU pragma: export
#include <GREM/core/data/ConstantString.hpp>                // IWYU pragma: export
#include <GREM/core/data/ConvexPolytope.hpp>                // IWYU pragma: export
#include <GREM/core/data/DoubleEndedQueue.hpp>              // IWYU pragma: export
#include <GREM/core/data/DoublyLinkedList.hpp>              // IWYU pragma: export
#include <GREM/core/data/Function.hpp>                      // IWYU pragma: export
#include <GREM/core/data/FunctionView.hpp>                  // IWYU pragma: export
#include <GREM/core/data/HashMap.hpp>                       // IWYU pragma: export
#include <GREM/core/data/HashSet.hpp>                       // IWYU pragma: export
#include <GREM/core/data/Indirect.hpp>                      // IWYU pragma: export
#include <GREM/core/data/InplaceArrayList.hpp>              // IWYU pragma: export
#include <GREM/core/data/InplaceBuffer.hpp>                 // IWYU pragma: export
#include <GREM/core/data/InplaceDoubleEndedQueue.hpp>       // IWYU pragma: export
#include <GREM/core/data/InplaceRingBuffer.hpp>             // IWYU pragma: export
#include <GREM/core/data/LinearBoundingVolumeHierarchy.hpp> // IWYU pragma: export
#include <GREM/core/data/LinearBuffer.hpp>                  // IWYU pragma: export
#include <GREM/core/data/LooseOrthtree.hpp>                 // IWYU pragma: export
#include <GREM/core/data/MPMCQueue.hpp>                     // IWYU pragma: export
#include <GREM/core/data/Optional.hpp>                      // IWYU pragma: export
#include <GREM/core/data/OrderedMap.hpp>                    // IWYU pragma: export
#include <GREM/core/data/OrderedMultimap.hpp>               // IWYU pragma: export
#include <GREM/core/data/Pair.hpp>                          // IWYU pragma: export
#include <GREM/core/data/PriorityQueue.hpp>                 // IWYU pragma: export
#include <GREM/core/data/RangeAllocator.hpp>                // IWYU pragma: export
#include <GREM/core/data/Registry.hpp>                      // IWYU pragma: export
#include <GREM/core/data/RingBuffer.hpp>                    // IWYU pragma: export
#include <GREM/core/data/SPSCQueue.hpp>                     // IWYU pragma: export
#include <GREM/core/data/SPSCSlidingWindowQueue.hpp>        // IWYU pragma: export
#include <GREM/core/data/SharedPointer.hpp>                 // IWYU pragma: export
#include <GREM/core/data/SinglyLinkedList.hpp>              // IWYU pragma: export
#include <GREM/core/data/SmallArrayList.hpp>                // IWYU pragma: export
#include <GREM/core/data/SmallBuffer.hpp>                   // IWYU pragma: export
#include <GREM/core/data/Span.hpp>                          // IWYU pragma: export
#include <GREM/core/data/StridedSpan.hpp>                   // IWYU pragma: export
#include <GREM/core/data/String.hpp>                        // IWYU pragma: export
#include <GREM/core/data/StringPool.hpp>                    // IWYU pragma: export
#include <GREM/core/data/StringView.hpp>                    // IWYU pragma: export
#include <GREM/core/data/Table.hpp>                         // IWYU pragma: export
#include <GREM/core/data/TriangleMesh.hpp>                  // IWYU pragma: export
#include <GREM/core/data/Tuple.hpp>                         // IWYU pragma: export
#include <GREM/core/data/UniqueHandle.hpp>                  // IWYU pragma: export
#include <GREM/core/data/UniquePointer.hpp>                 // IWYU pragma: export
#include <GREM/core/data/Variant.hpp>                       // IWYU pragma: export
#include <GREM/core/debug_formatting.hpp>                   // IWYU pragma: export
#include <GREM/core/extents.hpp>                            // IWYU pragma: export
#include <GREM/core/formats/Adler32.hpp>                    // IWYU pragma: export
#include <GREM/core/formats/CRC32.hpp>                      // IWYU pragma: export
#include <GREM/core/formats/ascii.hpp>                      // IWYU pragma: export
#include <GREM/core/formats/base16.hpp>                     // IWYU pragma: export
#include <GREM/core/formats/base64.hpp>                     // IWYU pragma: export
#include <GREM/core/formats/deflate.hpp>                    // IWYU pragma: export
#include <GREM/core/formats/gltf.hpp>                       // IWYU pragma: export
#include <GREM/core/formats/json.hpp>                       // IWYU pragma: export
#include <GREM/core/formats/obj.hpp>                        // IWYU pragma: export
#include <GREM/core/formats/unicode.hpp>                    // IWYU pragma: export
#include <GREM/core/formats/uri.hpp>                        // IWYU pragma: export
#include <GREM/core/formats/xml.hpp>                        // IWYU pragma: export
#include <GREM/core/formatting.hpp>                         // IWYU pragma: export
#include <GREM/core/fundamentals.hpp>                       // IWYU pragma: export
#include <GREM/core/geometry.hpp>                           // IWYU pragma: export
#include <GREM/core/math.hpp>                               // IWYU pragma: export
#include <GREM/core/metaprogramming.hpp>                    // IWYU pragma: export
#include <GREM/core/profiling.hpp>                          // IWYU pragma: export
#include <GREM/core/randomness.hpp>                         // IWYU pragma: export
#include <GREM/core/statistics.hpp>                         // IWYU pragma: export
#include <GREM/core/system/Clock.hpp>                       // IWYU pragma: export
#include <GREM/core/system/File.hpp>                        // IWYU pragma: export
#include <GREM/core/system/Filesystem.hpp>                  // IWYU pragma: export
#include <GREM/core/system/NativeFilesystem.hpp>            // IWYU pragma: export
#include <GREM/core/system/NativeInputFile.hpp>             // IWYU pragma: export
#include <GREM/core/system/NativeOutputFile.hpp>            // IWYU pragma: export
#include <GREM/core/system/SharedLibrary.hpp>               // IWYU pragma: export
#include <GREM/core/system/Thread.hpp>                      // IWYU pragma: export
#include <GREM/core/system/synchronization.hpp>             // IWYU pragma: export
#include <GREM/core/time.hpp>                               // IWYU pragma: export
#include <GREM/core/version.hpp>                            // IWYU pragma: export

#endif
