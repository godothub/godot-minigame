#!/usr/bin/env python3
import os
import sys

def embed_resources(sources, target):
    with open(target, 'w') as f:
        f.write('#include "resources/embedded_resources.h"\n\n')
        f.write('namespace toolkit {\n')
        f.write('namespace resources {\n\n')
        
        # Generate data arrays
        for source in sources:
            if not os.path.exists(source):
                continue
                
            # Get relative path and create variable name
            rel_path = os.path.relpath(source).replace('\\', '/')
            var_name = rel_path.replace('/', '_').replace('\\', '_').replace('.', '_')
            
            # Read file data
            with open(source, 'rb') as data_file:
                data = data_file.read()
            
            # Generate C++ array
            f.write(f'// Embedded resource: {rel_path}\n')
            f.write(f'const unsigned char embedded_{var_name}_data[] = {{\n')
            
            # Write bytes in chunks of 16
            for i in range(0, len(data), 16):
                chunk = data[i:i+16]
                hex_bytes = ', '.join(f'0x{b:02x}' for b in chunk)
                f.write(f'    {hex_bytes},\n')
            
            f.write(f'}};\n')
            f.write(f'const size_t embedded_{var_name}_size = {len(data)};\n\n')
        
        # Generate resource map
        f.write('const EmbeddedResourceEntry embedded_resources[] = {\n')
        for source in sources:
            if not os.path.exists(source):
                continue
                
            rel_path = os.path.relpath(source).replace('\\', '/')
            var_name = rel_path.replace('/', '_').replace('\\', '_').replace('.', '_')
            f.write(f'    {{"{rel_path.replace(chr(92), "/")}", embedded_{var_name}_data, embedded_{var_name}_size}},\n')
        
        f.write('    {nullptr, nullptr, 0} // Terminator\n')
        f.write('};\n\n')
        
        f.write('} // namespace resources\n')
        f.write('} // namespace toolkit\n')

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: embed_resources.py <source1> [source2...] <target>")
        sys.exit(1)
    
    sources = sys.argv[1:-1]
    target = sys.argv[-1]
    
    # Ensure target directory exists
    os.makedirs(os.path.dirname(target), exist_ok=True)
    
    embed_resources(sources, target)