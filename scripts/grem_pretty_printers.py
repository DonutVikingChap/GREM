# SPDX-FileCopyrightText: 2026 Ivar Härnqvist
# SPDX-License-Identifier: MIT

import gdb
import gdb.printing
import re

def vis(value):
	# "Visualize" a gdb.Value.
	# This function is really just a convoluted way of removing the " = {...}"
	# that str(value) adds after structs since some recent version of GDB, while
	# taking care to match the number of "{" and "}" brackets so that nested
	# structs are skipped properly.
	s = value.format_string(static_members=False)
	parts = []
	i = 0
	end = len(s)
	while i < end:
		j = s.find(' = {', i)
		if j == -1:
			parts.append(s[i:])
			break
		parts.append(s[i:j])
		i = j
		j += 4
		level = 1
		while j < end:
			if s[j] == '{':
				level += 1
			elif s[j] == '}':
				level -= 1
				if level == 0:
					j += 1
					break
			j += 1
		if j != end or level != 0:
			parts.append(s[i:j])
		i = j
	result = ''.join(parts)
	return result if result != '' else s

def get_physics_quantity_unit_symbol_string(unit_type):
	def get_dimension_type(unit_type):
		try:
			return unit_type['DIMENSION'].type.unqualified().strip_typedefs()
		except:
			return unit_type.template_argument(0).type.unqualified().strip_typedefs()

	def get_magnitude_type(unit_type):
		try:
			return unit_type['MAGNITUDE'].type.unqualified().strip_typedefs()
		except:
			return unit_type.template_argument(1).type.unqualified().strip_typedefs()

	def get_magnitude_type_value(magnitude_type):
		try:
			return magnitude_type['VALUE']
		except:
			return magnitude_type.template_argument(0)

	def format_base_unit(result, exponent, string):
		if exponent != 0:
			result += string
			if exponent != 1:
				result += f"^{exponent}"
		return result

	check_for_base_class = True
	while check_for_base_class:
		check_for_base_class = False
		for field in unit_type.fields():
			if field.is_base_class:
				unit_type = field.type.strip_typedefs()
				check_for_base_class = True
				break
	dimension_type = get_dimension_type(unit_type)
	magnitude_type = get_magnitude_type(unit_type)
	mass = int(dimension_type.template_argument(0))
	length = int(dimension_type.template_argument(1))
	time = int(dimension_type.template_argument(2))
	magnitude_value = get_magnitude_type_value(magnitude_type)
	result = ''
	if magnitude_value == 1.0:
		standard_unit_symbol_strings = {
			(0, 0, 0): '', # Dimensionless.
			(1, 0, 0): ' kg', # Mass.
			(0, 1, 0): ' m', # Length.
			(0, 0, 1): ' s', # Time.
			(0, 2, 0): ' m^2', # Area.
			(0, 3, 0): ' m^3', # Volume.
			(1, -3, 0): ' kg/m^3', # Density.
			(0, 0, -1): ' Hz', # Frequency.
			(0, 1, 1): ' m s', # Absement.
			(0, -1, 0): ' m^-1', # Wavenumber.
			(0, 1, -1): ' m/s', # Velocity.
			(0, 1, -2): ' m/s^2', # Acceleration.
			(1, 1, -2): ' N', # Force.
			(1, 2, -2): ' N m', # Torque.
			(1, 1, -1): ' N s', # Impulse.
			(1, 2, 0): ' kg m^2', # Moment of inertia.
			(0, 0, -2): ' rad/s^2', # Angular acceleration.
			(1, 2, -1): ' N m s', # Angular impulse.
			(1, 0, -1): ' kg/s', # Mass flow rate.
			(0, 3, -1): ' m^3/s', # Volumetric flow rate.
			(1, 2, -2): ' J', # Work.
			(1, 2, -3): ' W', # Power.
		}
		standard_unit_symbol_string = standard_unit_symbol_strings.get((mass, length, time))
		if standard_unit_symbol_string is not None:
			return standard_unit_symbol_string
	else:
		result = f" ({float(magnitude_value)}"
	result = format_base_unit(result, mass, ' kg')
	result = format_base_unit(result, length, ' m')
	result = format_base_unit(result, time, ' s')
	if magnitude_value != 1.0:
		result += ')'
	return result

class AllocationPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		elements = self.val['elements']
		size = self.val['elementCount']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(elements[i]) for i in range(int(size))) + ']'

	def children(self):
		elements = self.val['elements']
		yield ('size()', self.val['elementCount'])
		yield ('get_allocator()', self.val['allocator'])
		for i in range(int(self.val['elementCount'])):
			yield (f"[{i}]", elements[i])

	def display_hint(self):
		return 'array'

class ArrayPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		elements = self.val['_private_elements']
		size = self.val.type.strip_typedefs().template_argument(1)
		if size > 3:
			return f"{{... ({size} items)}}"
		return '[' + ', '.join(vis(elements[i]) for i in range(int(size))) + ']'

	def children(self):
		size = self.val.type.strip_typedefs().template_argument(1)
		yield ('size()', size)
		if size != 0:
			elements = self.val['_private_elements']
			for i in range(int(size)):
				yield (f"[{i}]", elements[i])

	def display_hint(self):
		return 'array'

class ArrayListPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		elements = self.val['elements']
		size = self.val['elementCount']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(elements[i]) for i in range(int(size))) + ']'

	def children(self):
		elements = self.val['elements']
		yield ('size()', self.val['elementCount'])
		yield ('capacity()', self.val['elementCapacity'])
		yield ('get_allocator()', self.val['allocator'])
		for i in range(int(self.val['elementCount'])):
			yield (f"[{i}]", elements[i])

	def display_hint(self):
		return 'array'

class BitArrayPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		size = self.val.type.strip_typedefs().template_argument(0)
		if size > 32:
			return f"[... ({size} items)]"
		elements = self.val['bits']
		b = elements[0]
		return '[' + ', '.join(str((b >> i) & 1) for i in range(int(size))) + ']'

	def children(self):
		size = self.val.type.strip_typedefs().template_argument(0)
		yield ('size()', size)
		elements = self.val['bits']
		n = elements[0].type.sizeof * 8
		for i in range(int(size)):
			yield (f"[{i}]", ((elements[i // n] >> (i % n)) & 1) != 0)

	def display_hint(self):
		return 'array'

class BitBufferPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		size = self.val['bitCount']
		if size > 32:
			return f"[... ({size} items)]"
		elements = self.val['bits']
		b = elements[0]
		return '[' + ', '.join(str((b >> i) & 1) for i in range(int(size))) + ']'

	def children(self):
		elements = self.val['bits']
		yield ('size()', self.val['bitCount'])
		yield ('capacity()', self.val['bitCapacity'])
		yield ('get_allocator()', self.val['allocator'])
		n = elements[0].type.sizeof * 8
		for i in range(int(self.val['bitCount'])):
			yield (f"[{i}]", ((elements[i // n] >> (i % n)) & 1) != 0)

	def display_hint(self):
		return 'array'

class BufferPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		elements = self.val['elements']
		size = self.val['elementCount']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(elements[i]) for i in range(int(size))) + ']'

	def children(self):
		elements = self.val['elements']
		yield ('size()', self.val['elementCount'])
		yield ('capacity()', self.val['elementCapacity'])
		yield ('get_allocator()', self.val['allocator'])
		for i in range(int(self.val['elementCount'])):
			yield (f"[{i}]", elements[i])

	def display_hint(self):
		return 'array'

class ConstantStringPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		characters = self.val['characters']
		return ''.join(chr(characters[i]) for i in range(int(self.val.type.strip_typedefs().template_argument(1))))

	def display_hint(self):
		return 'string'

class CRC32Printer:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return f"0x{int(self.val['value']):X}"

	def children(self):
		yield ('uint32_t()', self.val['value'])

class CStringViewPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return self.val['string'].string()

	def display_hint(self):
		return 'string'

class DoubleEndedQueuePrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		buffer = self.val['ringBuffer']
		begin = self.val['beginIndex']
		size = self.val['endIndex'] - begin
		mask = self.val['mask']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(buffer[(begin + i) & mask]) for i in range(int(size))) + ']'

	def children(self):
		buffer = self.val['ringBuffer']
		begin = self.val['beginIndex']
		size = self.val['endIndex'] - begin
		mask = self.val['mask']
		yield ('size()', size)
		yield ('capacity()', 0 if mask == 0 else mask + 1)
		yield ('get_allocator()', self.val['allocator'])
		for i in range(int(size)):
			yield (f"[{i}]", buffer[(begin + i) & mask])

	def display_hint(self):
		return 'array'

class HashMapPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		activeBits = self.val['activeBits']
		keys = self.val['keys']
		values = self.val['values']
		size = self.val['elementCount']
		if size > 3:
			return f"{{... ({size} items)}}"
		return '{' + ', '.join(f"{vis(keys[i])}: {vis(values[i])}" for i in range(int(self.val['elementCapacity'])) if (activeBits[i // 64] & (1 << (i % 64))) != 0) + '}'

	def children(self):
		activeBits = self.val['activeBits']
		keys = self.val['keys']
		values = self.val['values']
		yield ('size()', self.val['elementCount'])
		yield ('capacity()', self.val['elementCapacity'])
		yield ('hash_function()', self.val['hash'])
		yield ('key_eq()', self.val['equal'])
		yield ('get_allocator()', self.val['allocator'])
		for i in range(int(self.val['elementCapacity'])):
			if (activeBits[i // 64] & (1 << (i % 64))) != 0:
				yield (f"[{keys[i]}]", values[i])

class HashSetPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		activeBits = self.val['activeBits']
		keys = self.val['keys']
		size = self.val['elementCount']
		if size > 3:
			return f"{{... ({size} items)}}"
		return '{' + ', '.join(vis(keys[i]) for i in range(int(self.val['elementCapacity'])) if (activeBits[i // 64] & (1 << (i % 64))) != 0) + '}'

	def children(self):
		activeBits = self.val['activeBits']
		keys = self.val['keys']
		yield ('size()', self.val['elementCount'])
		yield ('capacity()', self.val['elementCapacity'])
		yield ('hash_function()', self.val['hash'])
		yield ('key_eq()', self.val['equal'])
		yield ('get_allocator()', self.val['allocator'])
		for i in range(int(self.val['elementCapacity'])):
			if (activeBits[i // 64] & (1 << (i % 64))) != 0:
				yield (f"[{keys[i]}]", keys[i])

class InplaceArrayListPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		t = self.val.type.strip_typedefs().template_argument(0).strip_typedefs()
		elements = self.val['buffer'].address.cast(t.pointer())
		size = self.val['elementCount']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(elements[i]) for i in range(int(size))) + ']'

	def children(self):
		t = self.val.type.strip_typedefs().template_argument(0).strip_typedefs()
		elements = self.val['buffer'].address.cast(t.pointer())
		yield ('size()', self.val['elementCount'])
		yield ('capacity()', self.val.type.strip_typedefs().template_argument(1))
		for i in range(int(self.val['elementCount'])):
			yield (f"[{i}]", elements[i])

	def display_hint(self):
		return 'array'

class InplaceBufferPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		t = self.val.type.strip_typedefs().template_argument(0).strip_typedefs()
		elements = self.val['elements']
		size = self.val['elementCount']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(elements[i]) for i in range(int(size))) + ']'

	def children(self):
		t = self.val.type.strip_typedefs().template_argument(0).strip_typedefs()
		elements = self.val['elements']
		yield ('size()', self.val['elementCount'])
		yield ('capacity()', self.val.type.strip_typedefs().template_argument(1))
		for i in range(int(self.val['elementCount'])):
			yield (f"[{i}]", elements[i])

	def display_hint(self):
		return 'array'

class InplaceDoubleEndedQueuePrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		buffer = self.val['ringBuffer']
		begin = self.val['beginIndex']
		size = self.val['endIndex'] - begin
		capacity = self.val.type.strip_typedefs().template_argument(1)
		mask = capacity - 1
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(buffer[(begin + i) & mask]['value']) for i in range(int(size))) + ']'

	def children(self):
		buffer = self.val['ringBuffer']
		begin = self.val['beginIndex']
		size = self.val['endIndex'] - begin
		capacity = self.val.type.strip_typedefs().template_argument(1)
		mask = capacity - 1
		yield ('size()', size)
		yield ('capacity()', capacity)
		for i in range(int(size)):
			yield (f"[{i}]", buffer[(begin + i) & mask]['value'])

	def display_hint(self):
		return 'array'

class InplaceRingBufferPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		buffer = self.val['ringBuffer']
		begin = self.val['beginIndex']
		size = self.val['endIndex'] - begin
		capacity = self.val.type.strip_typedefs().template_argument(1)
		mask = capacity - 1
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(buffer[(begin + i) & mask]) for i in range(int(size))) + ']'

	def children(self):
		buffer = self.val['ringBuffer']
		begin = self.val['beginIndex']
		size = self.val['endIndex'] - begin
		capacity = self.val.type.strip_typedefs().template_argument(1)
		mask = capacity - 1
		yield ('size()', size)
		yield ('capacity()', capacity)
		for i in range(int(size)):
			yield (f"[{i}]", buffer[(begin + i) & mask])

	def display_hint(self):
		return 'array'

class VecPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		n = self.val.type.strip_typedefs().template_argument(0)
		if n == 1:
			return f"({vis(self.val['x'])})"
		if n == 2:
			return f"({vis(self.val['x'])}, {vis(self.val['y'])})"
		if n == 3:
			return f"({vis(self.val['x'])}, {vis(self.val['y'])}, {vis(self.val['z'])})"
		if n == 4:
			return f"({vis(self.val['x'])}, {vis(self.val['y'])}, {vis(self.val['z'])}, {vis(self.val['w'])})"
		return None

	def children(self):
		n = self.val.type.strip_typedefs().template_argument(0)
		if n == 1:
			yield ('x', self.val['x'])
		if n == 2:
			yield ('x', self.val['x'])
			yield ('y', self.val['y'])
		elif n == 3:
			yield ('x', self.val['x'])
			yield ('y', self.val['y'])
			yield ('z', self.val['z'])
		elif n == 4:
			yield ('x', self.val['x'])
			yield ('y', self.val['y'])
			yield ('z', self.val['z'])
			yield ('w', self.val['w'])

class MatPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		c = self.val.type.strip_typedefs().template_argument(0)
		r = self.val.type.strip_typedefs().template_argument(1)
		result = '['
		for x in range(int(c)):
			if x > 0:
				result += ', '
			columns = self.val['_private_value']
			if r == 1:
				result += f"({vis(columns[0])}"
			elif r == 2:
				result += f"({vis(columns[0])}, {vis(columns[1])})"
			elif r == 3:
				result += f"({vis(columns[0])}, {vis(columns[1])}, {vis(columns[2])})"
			elif r == 4:
				result += f"({vis(columns[0])}, {vis(columns[1])}, {vis(columns[2])}, {vis(columns[3])})"
		result += ']'
		return result

	def children(self):
		for x in range(int(self.val.type.strip_typedefs().template_argument(0))):
			yield (f"[{x}]", self.val['_private_value'][x])

	def display_hint(self):
		return 'array'

class QuaPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return f"{vis(self.val['x'])}i + {vis(self.val['y'])}j + {vis(self.val['z'])}k + {vis(self.val['w'])}"

	def children(self):
		yield ('x', self.val['x'])
		yield ('y', self.val['y'])
		yield ('z', self.val['z'])
		yield ('w', self.val['w'])

class OptionalPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		if self.val['variant']['activeTypeIndex'] == 0:
			return 'nullopt'
		return vis(self.val['variant']['storage']['tail']['head'])

	def children(self):
		has_value = self.val['variant']['activeTypeIndex'] != 0
		yield ('has_value()', has_value)
		if has_value:
			yield ('value()', self.val['variant']['storage']['tail']['head'])

class OrderedMultimapPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		elements = self.val['elements']
		size = self.val['elementCount']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(f"{vis(elements[i]['first'])}: {vis(elements[i]['second'])}" for i in range(int(size))) + ']'

	def children(self):
		t = self.val.type.strip_typedefs().template_argument(0).strip_typedefs()
		elements = self.val['elements']
		yield ('size()', self.val['elementCount'])
		yield ('key_comp()', self.val['compare']['comp'])
		yield ('value_comp()', self.val['compare'])
		yield ('get_allocator()', self.val['allocator'])
		for i in range(int(self.val['elementCount'])):
			yield (f"[{elements[i]['first']}]", elements[i]['second'])

	def display_hint(self):
		return 'array'

class PairPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return f"({vis(self.val['first'])}, {vis(self.val['second'])})"

	def children(self):
		yield ('first', self.val['first'])
		yield ('second', self.val['second'])

class RegistryElementIDPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		value = int(self.val['value'])
		if value == 0xFFFFFFFF:
			return 'INVALID'
		index = value & 0xFFFF
		generation = (value >> 16) & 0x3FFF
		flags = (value >> 30) & 0x3
		return f"{{index = {index}, generation = {generation}, flags = 0b{flags:02b}}}"

	def children(self):
		value = int(self.val['value'])
		index = value & 0xFFFF
		generation = (value >> 16) & 0x3FFF
		flags = (value >> 30) & 0x3
		yield ('index', index)
		yield ('generation', generation)
		yield ('flags', flags)

class RingBufferPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		buffer = self.val['ringBuffer']
		begin = self.val['beginIndex']
		size = self.val['endIndex'] - begin
		mask = self.val['mask']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(buffer[(begin + i) & mask]) for i in range(int(size))) + ']'

	def children(self):
		buffer = self.val['ringBuffer']
		begin = self.val['beginIndex']
		size = self.val['endIndex'] - begin
		mask = self.val['mask']
		yield ('size()', size)
		yield ('capacity()', 0 if mask == 0 else mask + 1)
		yield ('get_allocator()', self.val['allocator'])
		for i in range(int(size)):
			yield (f"[{i}]", buffer[(begin + i) & mask])

	def display_hint(self):
		return 'array'

class SharedPointerPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return vis(self.val['object'])

	def children(self):
		if self.val['object'] == 0:
			yield ('get()', self.val['object'])
			return
		header = self.val['controlBlock'].dereference()
		yield ('(total reference count)', header['totalReferenceCount'])
		yield ('use_count()', header['strongReferenceCount'])
		yield ('get()', self.val['object'])

class WeakPointerPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		if self.val['object'] == 0:
			return vis(self.val['object'])
		header = self.val['controlBlock'].dereference()
		if header['strongReferenceCount'] == 0:
			return 'expired'
		return vis(self.val['object'])

	def children(self):
		if self.val['object'] == 0:
			yield ('get()', self.val['object'])
			return
		header = self.val['controlBlock'].dereference()
		yield ('(total reference count)', header['totalReferenceCount'])
		yield ('use_count()', header['strongReferenceCount'])
		if header['strongReferenceCount'] != 0:
			yield ('get()', self.val['object'])

class SmallArrayListPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		elements = self.val['elements']
		size = self.val['elementCount']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(elements[i]) for i in range(int(size))) + ']'

	def children(self):
		elements = self.val['elements']
		yield ('size()', self.val['elementCount'])
		yield ('capacity()', self.val['elementCapacity'])
		yield ('get_allocator()', self.val['allocator'])
		for i in range(int(self.val['elementCount'])):
			yield (f"[{i}]", elements[i])

	def display_hint(self):
		return 'array'

class SmallBufferPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		elements = self.val['elements']
		size = self.val['elementCount']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(elements[i]) for i in range(int(size))) + ']'

	def children(self):
		elements = self.val['elements']
		yield ('size()', self.val['elementCount'])
		yield ('capacity()', self.val['elementCapacity'])
		yield ('get_allocator()', self.val['allocator'])
		for i in range(int(self.val['elementCount'])):
			yield (f"[{i}]", elements[i])

	def display_hint(self):
		return 'array'

class SpanPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		elements = self.val['elements']
		size = self._get_span_size()
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(elements[i]) for i in range(int(size))) + ']'

	def children(self):
		elements = self.val['elements']
		size = self._get_span_size()
		yield ('data()', elements)
		yield ('size()', size)
		for i in range(int(size)):
			yield (f"[{i}]", elements[i])

	def display_hint(self):
		return 'array'

	def _get_span_size(self):
		try:
			return self.val['elementCount']
		except:
			return self.val.type.strip_typedefs().template_argument(1)

class StridedSpanPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		size = self._get_strided_span_size()
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(vis(element) for element in self._get_strided_span_elements()) + ']'

	def children(self):
		yield ('base()', self.val['elements'])
		yield ('size()', self._get_strided_span_size())
		yield ('stride()', self._get_strided_span_stride())
		for i, element in enumerate(self._get_strided_span_elements()):
			yield (f"[{i}]", element)

	def display_hint(self):
		return 'array'

	def _get_strided_span_size(self):
		try:
			return self.val['elementCount']
		except:
			return self.val.type.strip_typedefs().template_argument(1)
		return 0

	def _get_strided_span_stride(self):
		try:
			return self.val['elementStride']
		except:
			return self.val.type.strip_typedefs().template_argument(2)
		return 0

	def _get_strided_span_elements(self):
		pointer_type = self.val.type.strip_typedefs().template_argument(0).pointer()
		element_bytes = self.val['elements'].cast(gdb.lookup_type('char').pointer())
		stride = self._get_strided_span_stride()
		for i in range(int(self._get_strided_span_size())):
			yield (element_bytes + i * stride).cast(pointer_type).dereference()

class TablePrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		size = self.val['rowCount']
		if size > 3:
			return f"[... ({size} items)]"
		return '[' + ', '.join(('[' + ', '.join(vis(element) for element in row) + ']') for row in self._get_table_rows()) + ']'

	def children(self):
		row_type = self.val.type.strip_typedefs().template_argument(0).strip_typedefs()
		yield ('size()', self.val['rowCount'])
		yield ('capacity()', self.val['rowCapacity'])
		yield ('get_allocator()', self.val['allocator'])
		for y, row in enumerate(self._get_table_rows()):
			for x, element in enumerate(row):
				yield (f"get<{x}>({y})", element)

	def display_hint(self):
		return 'array'

	def _get_table_rows(self):
		size = self.val['rowCount']
		for i in range(int(size)):
			row = []
			storage = self.val['columns']['storage']
			while True:
				row.append(storage['head'][i])
				try:
					storage = storage['tail']
					storage['head']
				except:
					break
			yield row

class TuplePrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return '(' + ', '.join(vis(field) for field in self._get_tuple_fields()) + ')'

	def children(self):
		for i, field in enumerate(self._get_tuple_fields()):
			yield (f"get<{i}>()", field)

	def _get_tuple_fields(self):
		storage = self.val['storage']
		while True:
			try:
				yield storage['head']
			except:
				break
			try:
				storage = storage['tail']
				storage['head']
			except:
				break

class UniqueHandlePrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return vis(self.val['handle'])

	def children(self):
		yield ('get()', self.val['handle'])
		yield ('get_deleter()', self.val['deleter'])

class UniquePointerPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return vis(self.val['handle'])

	def children(self):
		yield ('get()', self.val['handle']['handle'])
		yield ('get_deleter()', self.val['handle']['deleter'])

class VariantPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		storage = self.val['storage']
		for _ in range(int(self.val['activeTypeIndex'])):
			storage = storage['tail']
		try:
			storage['tail']['head']
		except:
			return 'valueless_by_exception'
		return vis(storage['head'])

	def children(self):
		index = int(self.val['activeTypeIndex'])
		yield ('index()', index)
		storage = self.val['storage']
		for _ in range(index):
			storage = storage['tail']
		try:
			storage['tail']['head']
		except:
			return
		value = storage['head']
		yield (f"get<{index}>()", value.address.cast(value.type.strip_typedefs().pointer()).dereference())

class JsonObjectPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		elements = self.val['membersSortedByName']['elements']
		size = self.val['membersSortedByName']['elementCount']
		if size > 3:
			return f"{{... ({size} items)}}"
		return '{' + ', '.join(f"{vis(elements[i]['first'])}: {vis(elements[i]['second'])}" for i in range(int(size))) + '}'

	def children(self):
		elements = self.val['membersSortedByName']['elements']
		size = self.val['membersSortedByName']['elementCount']
		yield ('size()', size)
		for i in range(int(size)):
			yield (f"[{elements[i]['first']}]", elements[i]['second'])

class JsonArrayPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return vis(self.val['values'])

	def children(self):
		elements = self.val['values']['elements']
		yield ('size()', self.val['values']['elementCount'])
		yield ('capacity()', self.val['values']['elementCapacity'])
		for i in range(int(self.val['values']['elementCount'])):
			yield (f"[{i}]", elements[i])

	def display_hint(self):
		return 'array'

class JsonVariantPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return VariantPrinter(self.val.address.cast(self.val.type.strip_typedefs().pointer()).dereference()).to_string()

	def children(self):
		for child in VariantPrinter(self.val.address.cast(self.val.type.strip_typedefs().pointer()).dereference()).children():
			yield child

class JsonValuePrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return VariantPrinter(self.val.address.cast(self.val.type.strip_typedefs().pointer()).dereference()).to_string()

	def children(self):
		for child in VariantPrinter(self.val.address.cast(self.val.type.strip_typedefs().pointer()).dereference()).children():
			yield child
		yield ('getSource()', self.val['source'])

class ExecutionEntityIDPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		value = int(self.val['value'])
		if value == 0xFFFFFFFFFFFFFFFF:
			return 'INVALID'
		index = value & 0xFFFFFFFF
		generation = (value >> 32) & 0xFFFF
		flags = (value >> 48) & 0xFFFF
		return f"{{index = {index}, generation = {generation}, flags = 0b{flags:016b}}}"

	def children(self):
		value = int(self.val['value'])
		index = value & 0xFFFFFFFF
		generation = (value >> 32) & 0xFFFF
		flags = (value >> 48) & 0xFFFF
		yield ('index', index)
		yield ('generation', generation)
		yield ('flags', flags)

def format_ipv4_address(sin_addr):
	s_addr = int(sin_addr['s_addr'])
	byte0 = (s_addr >> 0) & 0xFF
	byte1 = (s_addr >> 8) & 0xFF
	byte2 = (s_addr >> 16) & 0xFF
	byte3 = (s_addr >> 24) & 0xFF
	return f"{byte0}.{byte1}.{byte2}.{byte3}"

class NetworkingIPv4AddressPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return format_ipv4_address(self.val['sin_addr'])

def format_ipv6_address(sin6_addr):
	values = sin6_addr.address.cast(gdb.lookup_type('unsigned short').pointer())
	result = ''
	longest_zero_sequence_begin = None
	longest_zero_sequence_length = 1
	current_zero_sequence_begin = None
	for i in range(8):
		if values[i] == 0:
			if current_zero_sequence_begin is None:
				current_zero_sequence_begin = i
		else:
			length = i - current_zero_sequence_begin
			if length > longest_zero_sequence_length:
				longest_zero_sequence_begin = current_zero_sequence_begin
				longest_zero_sequence_length = length

	if longest_zero_sequence_begin is None:
		return ':'.join(f'{values[i]:x}' for i in range(8))
	return ':'.join(f'{values[i]:x}' for i in range(longest_zero_sequence_begin)) + '::' + ':'.join(f'{values[i]:x}' for i in range(longest_zero_sequence_begin + longest_zero_sequence_length, 8))

class NetworkingIPv6AddressPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return format_ipv6_address(self.val['sin6_addr'])

class NetworkingIPv4EndpointPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		result = format_ipv4_address(self.val['addr']['sin_addr'])
		port = int(self.val['addr']['sin_port'])
		port = ((port >> 8) & 0xFF) | ((port & 0xFF) << 8)
		if port != 0:
			result += f":{port}"
		return result

class NetworkingIPv6EndpointPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		result = '[' + format_ipv6_address(int(self.val['addr']['sin6_addr'])) + ']'
		port = int(self.val['addr']['sin6_port'])
		port = ((port >> 8) & 0xFF) | ((port & 0xFF) << 8)
		if port != 0:
			result += f":{port}"
		return result

class NetworkingEndpointPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		family = self.val['storage'].address.cast(gdb.lookup_type('short').pointer()).dereference()
		if family == 2:
			addr = self.val['storage'].address.cast(gdb.lookup_type('struct sockaddr_in').pointer()).dereference()
			result = format_ipv4_address(addr['sin_addr'])
			port = int(addr['sin_port'])
			port = ((port >> 8) & 0xFF) | ((port & 0xFF) << 8)
			if port != 0:
				result += f":{port}"
			return result
		addr = self.val['storage'].address.cast(gdb.lookup_type('struct sockaddr_in6').pointer()).dereference()
		result = '[' + format_ipv6_address(int(addr['sin6_addr'])) + ']'
		port = int(addr['sin6_port'])
		port = ((port >> 8) & 0xFF) | ((port & 0xFF) << 8)
		if port != 0:
			result += f":{port}"
		return result

class PhysicsZeroPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return '0'

class PhysicsQuantityPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		n = self.val.type.strip_typedefs().template_argument(0)
		components = self.val['_private_value'].address.cast(gdb.lookup_type('float').pointer())
		result = '0'
		if n == 1:
			result = vis(components[0])
		elif n == 2:
			result = f"({vis(components[0])}, {vis(components[1])})"
		elif n == 3:
			result = f"({vis(components[0])}, {vis(components[1])}, {vis(components[2])})"
		try:
			unit_type = self.val.type.strip_typedefs().template_argument(1).strip_typedefs()
		except:
			return result
		return result + get_physics_quantity_unit_symbol_string(unit_type)

	def children(self):
		n = self.val.type.strip_typedefs().template_argument(0)
		components = self.val['_private_value'].address.cast(gdb.lookup_type('float').pointer())
		if n == 2:
			yield ('x', components[0])
			yield ('y', components[1])
		elif n == 3:
			yield ('x', components[0])
			yield ('y', components[1])
			yield ('z', components[2])

class PhysicsTensorQuantityPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		n = self.val.type.strip_typedefs().template_argument(0)
		components = self.val['_private_value'].address.cast(gdb.lookup_type('float').pointer())
		result = '0'
		if n == 1:
			result = f"[({vis(components[0])})]"
		elif n == 2:
			result = f"[({vis(components[0])}, {vis(components[1])}), ({vis(components[2])}, {vis(components[3])})]"
		elif n == 3:
			result = f"[({vis(components[0])}, {vis(components[1])}, {vis(components[2])}), ({vis(components[3])}, {vis(components[4])}, {vis(components[5])}), ({vis(components[6])}, {vis(components[7])}, {vis(components[8])})]"
		try:
			unit_type = self.val.type.strip_typedefs().template_argument(1).strip_typedefs()
		except:
			return result
		return result + get_physics_quantity_unit_symbol_string(unit_type)

	def children(self):
		n = self.val.type.strip_typedefs().template_argument(0)
		components = self.val['_private_value'].address.cast(gdb.lookup_type('float').pointer())
		if n == 1:
			yield ('[0][0]', components[0])
		if n == 2:
			for y in range(2):
				for x in range(2):
					yield (f"[{y}][{x}]", components[y * 2 + x])
		elif n == 3:
			for y in range(3):
				for x in range(3):
					yield (f"[{y}][{x}]", components[y * 3 + x])

class PhysicsOrientationPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		n = self.val.type.strip_typedefs().template_argument(0)
		if n == 3:
			return f"{vis(self.val['_private_value']['x'])}i + {vis(self.val['_private_value']['y'])}j + {vis(self.val['_private_value']['z'])}k + {vis(self.val['_private_value']['w'])}"
		return f"{vis(self.val['_private_value'])} rad"

	def children(self):
		n = self.val.type.strip_typedefs().template_argument(0)
		if n == 3:
			yield ('x', self.val['_private_value']['x'])
			yield ('y', self.val['_private_value']['y'])
			yield ('z', self.val['_private_value']['z'])
			yield ('w', self.val['_private_value']['w'])

pp = None

def register_grem_pretty_printers(objfile):
	global pp
	gdb.printing.register_pretty_printer(objfile, pp)

def init():
	global pp
	pp = gdb.printing.RegexpCollectionPrettyPrinter("GREM")

	pp.add_printer('grem::Allocation', '^(grem::)?(pmr::)?Allocation<.*>$', AllocationPrinter)
	pp.add_printer('grem::Array', '^(grem::)?Array<.*>$', ArrayPrinter)
	pp.add_printer('grem::ArrayList', '^(grem::)?(pmr::)?ArrayList<.*>$', ArrayListPrinter)
	pp.add_printer('grem::BitArray', '^(grem::)?BitArray<.*>$', BitArrayPrinter)
	pp.add_printer('grem::BitBuffer', '^(grem::)?(pmr::)?BitBuffer(Base<.*>)?$', BitBufferPrinter)
	pp.add_printer('grem::Buffer', '^(grem::)?(pmr::)?Buffer<.*>$', BufferPrinter)
	pp.add_printer('grem::ConstantString', '^(grem::)?ConstantString<.*>$', ConstantStringPrinter)
	pp.add_printer('grem::CRC32', '^(grem::)?CRC32$', CRC32Printer)
	pp.add_printer('grem::CStringView', '^(grem::)?CStringView(Base<.*>)?$', CStringViewPrinter)
	pp.add_printer('grem::DoubleEndedQueue', '^(grem::)?DoubleEndedQueue<.*>$', DoubleEndedQueuePrinter)
	pp.add_printer('grem::HashMap', '^(grem::)?(pmr::)?HashMap<.*>$', HashMapPrinter)
	pp.add_printer('grem::HashSet', '^(grem::)?(pmr::)?HashSet<.*>$', HashSetPrinter)
	pp.add_printer('grem::InplaceArrayList', '^(grem::)?InplaceArrayList<.*>$', InplaceArrayListPrinter)
	pp.add_printer('grem::InplaceBuffer', '^(grem::)?InplaceBuffer<.*>$', InplaceBufferPrinter)
	pp.add_printer('grem::InplaceDoubleEndedQueue', '^(grem::)?InplaceDoubleEndedQueue<.*>$', InplaceDoubleEndedQueuePrinter)
	pp.add_printer('grem::InplaceRingBuffer', '^(grem::)?InplaceRingBuffer<.*>$', InplaceRingBufferPrinter)
	pp.add_printer('grem::vec', '^(grem::)?vec(2|3|4|<.*>)$', VecPrinter)
	pp.add_printer('grem::mat', '^(grem::)?mat(2|3|4|<.*>)$', MatPrinter)
	pp.add_printer('grem::qua', '^(grem::)?qua(t|<.*>)$', QuaPrinter)
	pp.add_printer('grem::Optional', '^(grem::)?Optional<.*>$', OptionalPrinter)
	pp.add_printer('grem::OrderedMultimap', '^(grem::)?Ordered(Multimap|Map)<.*>$', OrderedMultimapPrinter)
	pp.add_printer('grem::Pair', '^(grem::)?Pair<.*>$', PairPrinter)
	pp.add_printer('grem::RegistryElementID', '^((grem::)?RegistryElementID(Base<.*>)?|(grem::)?(graphics|gfx)::(LightID|DecalID))$', RegistryElementIDPrinter)
	pp.add_printer('grem::RingBuffer', '^(grem::)?(pmr::)?RingBuffer<.*>$', RingBufferPrinter)
	pp.add_printer('grem::SharedPointer', '^(grem::)?SharedPointer<.*>$', SharedPointerPrinter)
	pp.add_printer('grem::WeakPointer', '^(grem::)?WeakPointer<.*>$', WeakPointerPrinter)
	pp.add_printer('grem::SmallArrayList', '^(grem::)?(pmr::)?SmallArrayList<.*>$', SmallArrayListPrinter)
	pp.add_printer('grem::SmallBuffer', '^(grem::)?(pmr::)?SmallBuffer<.*>$', SmallBufferPrinter)
	pp.add_printer('grem::Span', '^(grem::)?Span<.*>$', SpanPrinter)
	pp.add_printer('grem::StridedSpan', '^(grem::)?StridedSpan<.*>$', StridedSpanPrinter)
	pp.add_printer('grem::Table', '^(grem::)?Table<.*>$', TablePrinter)
	pp.add_printer('grem::Tuple', '^(grem::)?Tuple<.*>$', TuplePrinter)
	pp.add_printer('grem::UniqueHandle', '^(grem::)?UniqueHandle<.*>$', UniqueHandlePrinter)
	pp.add_printer('grem::UniquePointer', '^(grem::)?UniquePointer<.*>$', UniquePointerPrinter)
	pp.add_printer('grem::Variant', '^(grem::)?Variant<.*>$', VariantPrinter)
	pp.add_printer('grem::json::Object', '^(grem::)?json::Object(Base<.*>)?$', JsonObjectPrinter)
	pp.add_printer('grem::json::Array', '^(grem::)?json::Array(Base<.*>)?$', JsonArrayPrinter)
	pp.add_printer('grem::json::Variant', '^(grem::)?json::Variant(Base<.*>)?$', JsonVariantPrinter)
	pp.add_printer('grem::json::Value', '^(grem::)?json::Value(Base<.*>)?$', JsonValuePrinter)
	pp.add_printer('grem::execution::EntityID', '^(grem::)?(execution|exec)::EntityID$', ExecutionEntityIDPrinter)
	pp.add_printer('grem::networking::IPv4Address', '^(grem::)?(networking|net)::IPv4Address$', NetworkingIPv4AddressPrinter)
	pp.add_printer('grem::networking::IPv6Address', '^(grem::)?(networking|net)::IPv6Address$', NetworkingIPv6AddressPrinter)
	pp.add_printer('grem::networking::IPv4Endpoint', '^(grem::)?(networking|net)::IPv4Endpoint$', NetworkingIPv4EndpointPrinter)
	pp.add_printer('grem::networking::IPv6Endpoint', '^(grem::)?(networking|net)::IPv6Endpoint$', NetworkingIPv6EndpointPrinter)
	pp.add_printer('grem::networking::Endpoint', '^(grem::)?(networking|net)::Endpoint$', NetworkingEndpointPrinter)
	pp.add_printer('grem::physics::Zero', '^(grem::)?(physics|phys)::Zero$', PhysicsZeroPrinter)
	pp.add_printer('grem::physics::Quantity', '^(grem::)?(physics|phys)::(Quantity|Direction)<.*>$', PhysicsQuantityPrinter)
	pp.add_printer('grem::physics::TensorQuantity', '^(grem::)?(physics|phys)::(TensorQuantity|(Orthonormal)?Basis)<.*>$', PhysicsTensorQuantityPrinter)
	pp.add_printer('grem::physics::Orientation', '^(grem::)?(physics|phys)::Orientation<.*>$', PhysicsOrientationPrinter)

init()
