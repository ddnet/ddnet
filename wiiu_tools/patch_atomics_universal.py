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

def patch_elf_universal(filepath):
    print(f"Running universal atomic patcher on {filepath}...")
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
    
    # Scan 4-byte aligned instructions, only inside .text section
    for i in range(text_offset, text_offset + text_size - 15, 4):
        # Read 4 consecutive instructions
        w0 = struct.unpack('>I', data[i:i+4])[0]
        w1 = struct.unpack('>I', data[i+4:i+8])[0]
        w2 = struct.unpack('>I', data[i+8:i+12])[0]
        w3 = struct.unpack('>I', data[i+12:i+16])[0]
        
        # 1. Check w0: lwarx RT, RA, RB
        # Opcode is 31 (0x1f << 26 = 0x7c000000), extended opcode is 20 (0x14 << 1 = 0x28)
        if (w0 & 0xfc0007fe) == 0x7c000028:
            rt = (w0 >> 21) & 0x1f
            ra = (w0 >> 16) & 0x1f
            rb = (w0 >> 11) & 0x1f
            
            # 2. Check w2: stwcx. RS, RA, RB
            # Opcode is 31, extended opcode is 150 (0x96 << 1 = 0x12c), and Rc=1 (0x12d)
            if (w2 & 0xfc0007ff) == 0x7c00012d:
                rs = (w2 >> 21) & 0x1f
                ra2 = (w2 >> 16) & 0x1f
                rb2 = (w2 >> 11) & 0x1f
                
                # Verify address registers match
                if ra == ra2 and rb == rb2:
                    # 3. Check w3: bne -16 (backward branch by 16 bytes)
                    # Opcode is 16 (0x10 << 26 = 0x40000000), BI/BO check for inequality.
                    # Commonly 0x40a2fff4 or 0x4082fff4.
                    if (w3 & 0xfc00ffff) == 0x4000fff4:
                        
                        # We found a loop!
                        # Replace w0 with: lwz rt, 0(rb) (assuming ra=0)
                        # lwz opcode is 32 (0x20 << 26 = 0x80000000)
                        new_w0 = 0x80000000 | (rt << 21) | (rb << 16)
                        
                        # w1 (addi) remains unchanged
                        
                        # Replace w2 with: stw rs, 0(rb) (assuming ra=0)
                        # stw opcode is 36 (0x24 << 26 = 0x90000000)
                        new_w2 = 0x90000000 | (rs << 21) | (rb << 16)
                        
                        # Replace w3 with: nop (0x60000000)
                        new_w3 = 0x60000000
                        
                        # Patch in data
                        data[i:i+4] = struct.pack('>I', new_w0)
                        data[i+8:i+12] = struct.pack('>I', new_w2)
                        data[i+12:i+16] = struct.pack('>I', new_w3)
                        
                        patched_count += 1
                        print(f"Patched loop at offset 0x{i:08x}:")
                        print(f"  Old: 0x{w0:08x} 0x{w1:08x} 0x{w2:08x} 0x{w3:08x}")
                        print(f"  New: 0x{new_w0:08x} 0x{w1:08x} 0x{new_w2:08x} 0x{new_w3:08x}")
                        
    with open(filepath, 'wb') as f:
        f.write(data)
    print(f"Success: Patched {patched_count} universal atomic loops.")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: patch_atomics_universal.py <file>")
        sys.exit(1)
    patch_elf_universal(sys.argv[1])
