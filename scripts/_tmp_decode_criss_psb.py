import struct
import sys


class Randomizer:
    def __init__(self, seed):
        self.a = 123456789
        self.b = 362436069
        self.c = 521288629
        self.d = seed

    def rand(self):
        value = ((self.a << 11) ^ self.a) & 0xFFFFFFFF
        self.a, self.b, self.c = self.b, self.c, self.d
        self.d = (self.d ^ value ^ ((value ^ (self.d >> 11)) >> 8)) & 0xFFFFFFFF
        return self.d

    def decrypt(self, data, offset, length):
        value = 0
        for index in range(length):
            if value == 0:
                value = self.rand()
            data[offset + index] ^= value & 0xFF
            value >>= 8


path = sys.argv[1]
data = bytearray(open(path, "rb").read())
randomizer = Randomizer(664172729)
randomizer.decrypt(data, 8, 36)
offsets = struct.unpack_from("<9I", data, 8)

if struct.unpack_from("<H", data, 6)[0] & 2:
    randomizer.decrypt(data, offsets[0], offsets[4] - offsets[0])

print("size:", len(data))
print("version, flags:", struct.unpack_from("<HH", data, 4))
print("core offsets:", offsets)
print("v4 tail:", struct.unpack_from("<3I", data, 44))
for label, offset in zip(
    ("encrypt", "names", "string_offsets", "strings", "chunk_offsets", "chunk_lengths", "chunk_data", "entries"),
    offsets,
):
    print(f"{label:16} {offset:10} {data[offset:offset + 16].hex()}")
