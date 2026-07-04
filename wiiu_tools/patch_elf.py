import struct
import sys

def get_text_section(data):
    if data[0:4] != b'\x7fELF':
        raise ValueError("Not an ELF file")
    e_shoff = struct.unpack('>I', data[32:36])[0]
    e_shentsize = struct.unpack('>H', data[46:48])[0]
    e_shnum = struct.unpack('>H', data[48:50])[0]
    e_shstrndx = struct.unpack('>H', data[50:52])[0]
    
    shstr_entry_offset = e_shoff + e_shstrndx * e_shentsize
    shstr_offset = struct.unpack('>I', data[shstr_entry_offset+16:shstr_entry_offset+20])[0]
    
    for i in range(e_shnum):
        entry_offset = e_shoff + i * e_shentsize
        sh_name_offset = struct.unpack('>I', data[entry_offset:entry_offset+4])[0]
        sh_offset = struct.unpack('>I', data[entry_offset+16:entry_offset+20])[0]
        sh_size = struct.unpack('>I', data[entry_offset+20:entry_offset+24])[0]
        
        # Read name
        name_bytes = bytearray()
        idx = shstr_offset + sh_name_offset
        while data[idx] != 0:
            name_bytes.append(data[idx])
            idx += 1
        name = name_bytes.decode(errors='ignore')
        
        if name == '.text':
            return sh_offset, sh_size
            
    raise ValueError(".text section not found")

def patch_elf(filepath):
    print(f"Patching {filepath}...")
    with open(filepath, 'rb') as f:
        data = bytearray(f.read())
    
    try:
        text_offset, text_size = get_text_section(data)
        print(f"Found .text section at offset 0x{text_offset:x}, size 0x{text_size:x}")
    except Exception as e:
        print(f"Failed to find .text section: {e}. Scanning entire file.")
        text_offset = 0
        text_size = len(data)

    patched_count = 0
    # Process 4 bytes at a time, aligned, only inside .text section
    for i in range(text_offset, text_offset + text_size - 3, 4):
        word = struct.unpack('>I', data[i:i+4])[0]
        # Match tweq: (word & 0xffe003ff) == 0x7c800008
        if (word & 0xffe003ff) == 0x7c800008:
            # Replace with nop (0x60000000)
            data[i:i+4] = struct.pack('>I', 0x60000000)
            patched_count += 1
            
    with open(filepath, 'wb') as f:
        f.write(data)
    print(f"Patched {patched_count} tweq instructions.")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: patch_elf.py <file>")
        sys.exit(1)
    patch_elf(sys.argv[1])
