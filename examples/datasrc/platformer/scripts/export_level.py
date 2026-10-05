# Example Platformer level export script (converts .blend -> .glb + .json)
#
# How to use:
#
# Alternative A:
# 1. Open the level's .blend file in Blender
# 2. Go to the Scripting tab
# 3. Open this file in the Text Editor window
# 4. Press the Run Script play button whenever you want to export
#
# Alternative B:
# 1. Open the levels/ folder containing the .blend file in a terminal or command prompt
# 2. Run: blender --background filename.blend --python ../scripts/export_level.py
#
# NOTE: The exported output is written to `../../../data/platformer/levels/`, relative to the .blend file.
#       To change this, edit "OUTPUT_FILEPATH" below.
#
# How it works:
#
# The .blend file is expected to have the following structure:
# - collections
#   - "Level Geometry" - Mesh objects that get exported to the .glb file, which can have these custom properties:
#     - (optional) "kineticFriction": Float (default: 0.707)
#     - (optional) "staticFriction": Float (default: 0.8)
#     - (optional) "rollingResistance": Float (default: 0.01)
#     - (optional) "restitution": Float (default: 0.5)
#     - (optional) "frictionCombine": String ("AVERAGE" | "MINIMUM" | "MAXIMUM" | "MULTIPLY", default: "MINIMUM")
#     - (optional) "restitutionCombine": String ("AVERAGE" | "MINIMUM" | "MAXIMUM" | "MULTIPLY", default: "MAXIMUM")
#   - "Props" - Instanced static objects with custom properties
#     - "model": String (model filepath to instance - the Blender object's actual mesh is ignored)
#     - (optional) "kineticFriction": Float (default: 0.707)
#     - (optional) "staticFriction": Float (default: 0.8)
#     - (optional) "rollingResistance": Float (default: 0.01)
#     - (optional) "restitution": Float (default: 0.5)
#     - (optional) "frictionCombine": String ("AVERAGE" | "MINIMUM" | "MAXIMUM" | "MULTIPLY", default: "MINIMUM")
#     - (optional) "restitutionCombine": String ("AVERAGE" | "MINIMUM" | "MAXIMUM" | "MULTIPLY", default: "MAXIMUM")
#   - "Physics Objects" - Instanced dynamic objects with custom properties
#     - "model": String (model filepath to instance - the Blender object's actual mesh is ignored)
#     - (optional) "linearVelocity": Float array[3] [m/s] (default: [0, 0, 0])
#     - (optional) "angularVelocity": Float array[3] [rad/s] (default: [0, 0, 0])
#     - (optional) "gravityAcceleration": Float array[3] [m/s^2] (default: [0, -9.82, 0])
#     - (optional) "surroundingFluidDensity": Float [kg/m^3] (default: 1.2041)
#     - (optional) "centerOfBuoyancy": Float array[3] [m] (default: [0, 0, 0])
#     - (optional) "mass": Float [kg] (default: auto)
#     - (optional) "centerOfMass": Float array[3] [m] (default: [0, 0, 0])
#     - (optional) "principalMomentsOfInertia": Float array[3] [kg m^2] (default: auto)
#     - (optional) "localInertiaOrientation": Float array[4] [rad] (default: [0, 0, 0, 1])
#     - (optional) "kineticFriction": Float (default: 0.707)
#     - (optional) "staticFriction": Float (default: 0.8)
#     - (optional) "rollingResistance": Float (default: 0.01)
#     - (optional) "restitution": Float (default: 0.5)
#     - (optional) "linearDrag": Float (default: 0.5)
#     - (optional) "angularDrag": Float (default: 0.5)
#     - (optional) "frictionCombine": String ("AVERAGE" | "MINIMUM" | "MAXIMUM" | "MULTIPLY", default: "MINIMUM")
#     - (optional) "restitutionCombine": String ("AVERAGE" | "MINIMUM" | "MAXIMUM" | "MULTIPLY", default: "MAXIMUM")
#   - "Collectibles" (Objects whose positions mark the positions of collectibles)
# - objects
#   - "Player Spawn" (Object whose position and angles determine the spawnpoint)
#   - (Sun/Point/Spot Light objects)
# - worlds
#   - "World" with shader nodes:
#     - "Sky": Environment Texture (shader node that determines the sky image)
#     - "Ambient": Color (shader node that determines the ambient light color)
#
# The exported structure is as follows:
# - filename.glb (Level geometry as a glTF 2.0 (Binary) model, including the custom properties defined above on its nodes)
# - filename.json
#   {
#     "bounds": {
#       "min": [x, y, z],
#       "max": [x, y, z]
#     },
#     "geometry": filepath,
#     "skyImage": filepath,
#     "ambientLight": [r, g, b],
#     "directionalLights": [
#       {
#         "angles": [pitch, yaw],
#         "intensity": [r, g, b]
#       },
#       ...
#     ],
#     "pointLights": [
#       {
#         "position": [x, y, z],
#         "intensity": [r, g, b]
#       },
#       ...
#     ],
#     "spotLights": [
#       {
#         "position": [x, y, z],
#         "angles": [pitch, yaw],
#         "innerConeAngle": angle,
#         "outerConeAngle": angle,
#         "intensity": [r, g, b]
#       },
#       ...
#     ],
#     "playerSpawn": {
#       "position": [x, y, z],
#       "angles": [pitch, yaw]
#     },
#     "props": [
#       {
#         "position": [x, y, z],
#         "orientation": [i, j, k, w],
#         "scale": [sx, sy, sz],
#         "model": filepath,
#         "kineticFriction": coefficient,
#         "staticFriction": coefficient,
#         "rollingResistance": coefficient,
#         "restitution": coefficient,
#         "frictionCombine": string,
#         "restitutionCombine": string
#       },
#       ...
#     ],
#     "physicsObjects": [
#       {
#         "position": [x, y, z],
#         "orientation": [i, j, k, w],
#         "scale": [sx, sy, sz],
#         "model": filepath,
#         "linearVelocity": [vx, vy, vz],
#         "angularVelocity": [wx, wy, wz],
#         "gravityAcceleration": [gx, gy, gz],
#         "surroundingFluidDensity": density,
#         "centerOfBuoyancy": [dx, dy, dz],
#         "mass": mass,
#         "centerOfMass": [dx, dy, dz],
#         "principalMomentsOfInertia": [Ixx, Iyy, Izz],
#         "localInertiaOrientation": [i, j, k, w],
#         "kineticFriction": coefficient,
#         "staticFriction": coefficient,
#         "rollingResistance": coefficient,
#         "restitution": coefficient,
#         "linearDrag": coefficient,
#         "angularDrag": coefficient,
#         "frictionCombine": string,
#         "restitutionCombine": string
#       },
#       ...
#     ],
#     "collectibles": [
#       [x, y, z],
#       ...
#     ]
#   }

import bpy
import json
import math
import mathutils
from pathlib import Path
from collections import defaultdict

OUTPUT_FILEPATH = '../../../data/platformer/levels'

def convert_position(position):
    return (position[0], position[2], -position[1])

def convert_orientation(orientation):
    return (orientation.x, orientation.z, -orientation.y, orientation.w)

def convert_angles(basis):
    direction = basis @ mathutils.Vector((0, 1, 0))
    return (-math.asin(-direction.z), math.atan2(-direction.x, direction.y))

def convert_light_angles(basis):
    direction = basis @ mathutils.Vector((0, 0, -1))
    return (-math.asin(-direction.z), math.atan2(-direction.x, direction.y))

def convert_sun_light_intensity(light):
    irradiance = light.energy
    illuminance = irradiance / 8.33333
    return tuple(light.color * illuminance * (2 ** light.exposure))

def convert_omni_light_intensity(light):
    radiant_intensity = light.energy
    luminous_intensity = radiant_intensity / 683
    return tuple(light.color * luminous_intensity * (2 ** light.exposure))

input_filepath = Path(bpy.data.filepath)
geometry_output_filepath = Path(OUTPUT_FILEPATH).joinpath(input_filepath.with_suffix('.glb').name)
json_output_filepath = Path(OUTPUT_FILEPATH).joinpath(input_filepath.with_suffix('.json').name)

print(f"Exporting level geometry to {geometry_output_filepath}...")

bpy.ops.export_scene.gltf(filepath=str(geometry_output_filepath), collection='Level Geometry', export_extras=True)

print(f"Exporting level JSON to {json_output_filepath}...")

level = defaultdict(list)

bounds_min = None
bounds_max = None
for object in bpy.data.objects:
    for local_corner in object.bound_box:
        corner = convert_position(object.matrix_world @ mathutils.Vector(local_corner))
        bounds_min = corner if bounds_min is None else (
            min(bounds_min[0], corner[0]),
            min(bounds_min[1], corner[1]),
            min(bounds_min[2], corner[2]),
        )
        bounds_max = corner if bounds_max is None else (
            max(bounds_max[0], corner[0]),
            max(bounds_max[1], corner[1]),
            max(bounds_max[2], corner[2]),
        )
level['bounds'] = { 'min': tuple(bounds_min), 'max': tuple(bounds_max) }

level['geometry'] = geometry_output_filepath.relative_to(geometry_output_filepath.parent.parent).as_posix()

if 'World' in bpy.data.worlds:
    world_shader_nodes = bpy.data.worlds['World'].node_tree.nodes

    if 'Sky' in world_shader_nodes:
        sky_image_filepath = Path(bpy.path.abspath(world_shader_nodes['Sky'].image.filepath))
        level['skyImage'] = sky_image_filepath.relative_to(sky_image_filepath.parent.parent).as_posix()

    if 'Ambient' in world_shader_nodes:
        level['ambientLight'] = tuple(world_shader_nodes['Ambient'].outputs[0].default_value[:3])

for object in bpy.data.objects:
    if object.type == 'LIGHT':
        light = object.data
        if light.type == 'SUN':
            level['directionalLights'].append({
                'angles': convert_light_angles(object.matrix_world.to_3x3()),
                'intensity': convert_sun_light_intensity(light),
            })
        elif light.type == 'POINT':
            level['pointLights'].append({
                'position': convert_position(object.location),
                'intensity': convert_omni_light_intensity(light),
            })
        elif light.type == 'SPOT':
            level['spotLights'].append({
                'position': convert_position(object.location),
                'angles': convert_light_angles(object.matrix_world.to_3x3()),
                'innerConeAngle': 0.5 * light.spot_size * (1.0 - light.spot_blend),
                'outerConeAngle': 0.5 * light.spot_size,
                'intensity': convert_omni_light_intensity(light),
            })

    if object.name == 'Player Spawn':
        level['playerSpawn'] = {
            'position': convert_position(object.location),
            'angles': convert_angles(object.matrix_world.to_3x3()),
        }

if 'Props' in bpy.data.collections:
    for object in bpy.data.collections['Props'].all_objects:
        prop = {
            'position': convert_position(object.location),
            'orientation': convert_orientation(object.rotation_quaternion),
            'scale': tuple(object.scale),
            'model': str(object['model']),
        }
        if 'kineticFriction' in object:
            prop['kineticFriction'] = float(object['kineticFriction'])
        if 'staticFriction' in object:
            prop['staticFriction'] = float(object['staticFriction'])
        if 'rollingResistance' in object:
            prop['rollingResistance'] = float(object['rollingResistance'])
        if 'restitution' in object:
            prop['restitution'] = float(object['restitution'])
        if 'frictionCombine' in object:
            prop['frictionCombine'] = float(object['frictionCombine'])
        if 'restitutionCombine' in object:
            prop['restitutionCombine'] = float(object['restitutionCombine'])
        level['props'].append(prop)

if 'Physics Objects' in bpy.data.collections:
    for object in bpy.data.collections['Physics Objects'].all_objects:
        physics_object = {
            'position': convert_position(object.location),
            'orientation': convert_orientation(object.rotation_quaternion),
            'scale': tuple(object.scale),
            'model': str(object['model']),
        }
        if 'linearVelocity' in object:
            linear_velocity = object['linearVelocity']
            physics_object['linearVelocity'] = (float(linear_velocity[0]), float(linear_velocity[1]), float(linear_velocity[2]))
        if 'angularVelocity' in object:
            angular_velocity = object['angularVelocity']
            physics_object['angularVelocity'] = (float(angular_velocity[0]), float(angular_velocity[1]), float(angular_velocity[2]))
        if 'gravityAcceleration' in object:
            gravity_acceleration = object['gravityAcceleration']
            physics_object['gravityAcceleration'] = (float(gravity_acceleration[0]), float(gravity_acceleration[1]), float(gravity_acceleration[2]))
        if 'surroundingFluidDensity' in object:
            physics_object['surroundingFluidDensity'] = float(object['surroundingFluidDensity'])
        if 'centerOfBuoyancy' in object:
            center_of_buoyancy = object['centerOfBuoyancy']
            physics_object['centerOfBuoyancy'] = (float(center_of_buoyancy[0]), float(center_of_buoyancy[1]), float(center_of_buoyancy[2]))
        if 'mass' in object:
            physics_object['mass'] = float(object['mass'])
        if 'centerOfMass' in object:
            center_of_mass = object['centerOfMass']
            physics_object['centerOfMass'] = (float(center_of_mass[0]), float(center_of_mass[1]), float(center_of_mass[2]))
        if 'principalMomentsOfInertia' in object:
            principal_moments_of_inertia = object['principalMomentsOfInertia']
            physics_object['principalMomentsOfInertia'] = (float(principal_moments_of_inertia[0]), float(principal_moments_of_inertia[1]), float(principal_moments_of_inertia[2]))
        if 'localInertiaOrientation' in object:
            local_inertia_orientation = object['localInertiaOrientation']
            physics_object['localInertiaOrientation'] = (float(local_inertia_orientation[0]), float(local_inertia_orientation[1]), float(local_inertia_orientation[2]), float(local_inertia_orientation[3]))
        if 'kineticFriction' in object:
            physics_object['kineticFriction'] = float(object['kineticFriction'])
        if 'staticFriction' in object:
            physics_object['staticFriction'] = float(object['staticFriction'])
        if 'rollingResistance' in object:
            physics_object['rollingResistance'] = float(object['rollingResistance'])
        if 'restitution' in object:
            physics_object['restitution'] = float(object['restitution'])
        if 'linearDrag' in object:
            physics_object['linearDrag'] = float(object['linearDrag'])
        if 'angularDrag' in object:
            physics_object['angularDrag'] = float(object['angularDrag'])
        if 'frictionCombine' in object:
            physics_object['frictionCombine'] = float(object['frictionCombine'])
        if 'restitutionCombine' in object:
            physics_object['restitutionCombine'] = float(object['restitutionCombine'])
        level['physicsObjects'].append(physics_object)

if 'Collectibles' in bpy.data.collections:
    level['collectibles'] = [convert_position(object.location) for object in bpy.data.collections['Collectibles'].all_objects]

with open(json_output_filepath, 'w') as f:
    f.write(json.dumps(level, indent='\t'))

print('Level successfully exported.')
