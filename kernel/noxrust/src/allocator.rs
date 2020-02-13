const BYTES_PER_SLICE: u32 = 4;
const SLICES_PER_BYTE: usize = 8;
pub const DEFAULT_ALLOC_ALIGNMENT: u8 = 4;

#[repr(C)]
pub struct Allocator {
    bitmap_addr: *mut u8,
    bitmap_size: usize,
    memory_addr: *mut u8,
    max_bytes: usize,
    bytes_allocated: usize,
}

impl Allocator {
    pub fn new(base_addr: *mut u8, max_bytes: usize) -> Result<Allocator, u32> {
        if max_bytes <= 64 {
            // First byte of slices reserved (32-bytes gone)
            // Require at least one byte worth of slices
            return Err(1244);
            //return Err("Not enough memory, this allocator requires at least 64 bytes".to_string());
        }

        let max_allocated_slices = max_bytes / (BYTES_PER_SLICE as usize);

        let bitmap_addr: *mut u8 = base_addr as *mut u8;
        let bitmap_size =
            (max_allocated_slices / 8) as usize + if max_allocated_slices % 8 > 0 { 1 } else { 0 };
        if bitmap_size == max_bytes {
            //return Err("Not enough memory to fit bitmap".to_string());
            return Err(16);
        }

        unsafe {
            let memory_addr = bitmap_addr.offset(bitmap_size as isize);

            Ok(Allocator {
                bitmap_addr,
                bitmap_size,
                memory_addr,
                max_bytes,
                bytes_allocated: 0,
            })
        }
    }

    pub fn alloc(&mut self, size: usize) -> Result<*mut usize, u32> {
        self.alloc_core(size, DEFAULT_ALLOC_ALIGNMENT)
    }

    pub fn aligned_alloc(&mut self, size: usize, alignment: u8) -> Result<*mut usize, u32> {
        if alignment % 2 != 0 {
            // panic!("aligned_alloc with alignment that isn't a multiple of 2");
        }

        if alignment > 16 {
            // Right now, because of the combination of prefix bytes + byte per slice
            // TODO: 32-bit alignment once we move to 64-bit support?
            panic!("aligned_alloc with alignment that isn't a multiple of 2");
        }

        self.alloc_core(size, alignment)
    }

    fn alloc_core(&mut self, size: usize, alignment: u8) -> Result<*mut usize, u32> {
        // Calculate number of slices necessary to store data
        let slice_count = (size / (BYTES_PER_SLICE as usize)) + 1 + // 1 slice for size
            if size % (BYTES_PER_SLICE as usize) > 0 { 1 } else { 0 };

        // println!("Slices required for alloc: {}", slice_count);

        // Do we need an extended size byte?
        if slice_count > usize::max_value() {
            //return Err("Requested size is too large".to_string());
            return Err(42);
        }

        // Now find slice_count clear consecutive bits in the bitmap
        // Propagate error if any
        let start_slice = self.get_first_available_slice(slice_count, alignment)?;

        let start_offset = start_slice * (BYTES_PER_SLICE as usize);

        self.mark_slices(start_slice, slice_count, true);

        unsafe {
            let ptr = self.memory_addr.add(start_offset);

            *ptr.offset(0) = ((slice_count >> 24) & 0xFF) as u8;
            *ptr.offset(1) = ((slice_count >> 16) & 0xFF) as u8;
            *ptr.offset(2) = ((slice_count >> 8) & 0xFF) as u8;
            *ptr.offset(3) = (slice_count & 0xFF) as u8;

            self.bytes_allocated += size;

            Ok(ptr.offset(4).cast())
        }
    }

    pub fn free(&mut self, pointer: *mut usize) -> bool {
        let u8_ptr = pointer as *mut u8;
        // Get slice count (Stored in 4 bytes preceeding the pointer)
        let num_slices: usize = unsafe {
            (*u8_ptr.offset(-4) as usize) << 24
                | (*u8_ptr.offset(-3) as usize) << 16
                | (*u8_ptr.offset(-2) as usize) << 8
                | (*u8_ptr.offset(-1) as usize)
        };

        unsafe {
            let real_ptr_start = pointer.offset(-4) as *mut u8;
            let relative_start = real_ptr_start.offset_from(self.memory_addr);
            let start_slice = relative_start / (BYTES_PER_SLICE as isize);

            self.mark_slices(start_slice as usize, num_slices, false);
        }

        // Note: -1 because of the size bytes
        let allocation_size = (num_slices - 1) * (BYTES_PER_SLICE as usize);
        //println!("Allocation size to free: {}", allocation_size);

        self.bytes_allocated -= allocation_size;

        true
    }

    pub fn get_bytes_allocated(&self) -> usize {
        self.bytes_allocated
    }

    pub fn get_max_bytes(&self) -> usize {
        self.max_bytes
    }

    pub fn get_first_available_slice(
        &self,
        requested_size: usize,
        alignment: u8,
    ) -> Result<usize, u32> {
        let mut i: usize = 1;

        // TODO: clear_bits_start should be usize to allow us to address all memory
        let mut clear_bits_start: isize = -1;
        let mut clear_bits_found = 0;

        let mut found_bits = false;
        while i < self.bitmap_size {
            let cur_byte: u8 = self.get_bitmap_byte(i as isize);

            if cur_byte == 0xFF {
                // No bits free, start over
                clear_bits_start = -1;
                clear_bits_found = 0;
            // println!("Full busy byte, start=-1,found=0");
            } else if cur_byte == 0 && self.is_slice_aligned(i * SLICES_PER_BYTE, alignment) {
                // All bits free!
                if clear_bits_start == -1 {
                    clear_bits_start = i as isize * 8;
                    clear_bits_found = 8;
                } else {
                    clear_bits_found += 8;
                }
            //                println!(
            //                    "Full free byte, start = {}, found={}",
            //                   clear_bits_start, clear_bits_found
            //                );
            } else {
                //println!("Partially free byte");
                // *Some* bits free!
                for j in (0..7).rev() {
                    if cur_byte & (1 << j) == 0 {
                        if clear_bits_start == -1 {
                            let start_candidate = (i as isize * 8) + (8 - j) - 1;
                            if self.is_slice_aligned(start_candidate as usize, alignment) {
                                clear_bits_start = start_candidate;
                            } else {
                                continue;
                            }
                        }

                        clear_bits_found += 1;

                        if clear_bits_found >= requested_size {
                            found_bits = true;
                            break; // Found a free block
                        }
                    } else {
                        clear_bits_start = -1;
                        clear_bits_found = 0;
                    }
                }
            }

            // Did we find a free block?
            if clear_bits_start != -1 && clear_bits_found >= requested_size {
                found_bits = true;
                break;
            }

            i += 1;
        }

        if !found_bits || clear_bits_found < requested_size {
            /*
            println!(
                "Could not find enough slices! found={},requested={}",
                clear_bits_found, requested_size
            );
            */
            //return Err("Could not find enough free slices".to_string());
            return Err(187);
        }

        return Ok(clear_bits_start as usize);
    }

    // public for testing
    pub fn mark_slices(&self, start: usize, count: usize, used: bool) {
        let start_byte: usize = start / 8;
        let start_remainder: u8 = (start % 8) as u8;
        let last_byte: usize = (start + count) / 8;
        let last_remainder: u8 = ((start + count) % 8) as u8;

        // Now to do some checking for cases
        if start_byte == last_byte {
            let count_in_byte = count as u8;
            //println!("Start + end in same byte!");
            // Start and end is in the same byte
            // Example: Start: 0, count = 7, end = 7
            //	| 1 1 1 1 1 1 1 0 | 0 0 0 0 0 0	0 0 |
            //	| _____Byte0_____ | _____Byte1_____ |
            let bits_to_set_as_bits = (1 << count) - 1; // Bit magic: 127
            if used {
                let value = bits_to_set_as_bits << (8 - start_remainder - count_in_byte);
                self.set_bitmap_or(start_byte as isize, value);
            } else {
                let value = !(bits_to_set_as_bits << (8 - start_remainder - count_in_byte));
                self.set_bitmap_and(start_byte as isize, value);
            }

            return;
        }

        if last_byte == start_byte + 1 && start_remainder == 0 && last_remainder == 0 {
            // println!("Slice block fits entirely in one byte!");
            // The block of slices fills exactly an entire byte
            self.set_bitmap(start_byte as isize, if used { 0xFF } else { 0 });
            return;
        }

        // Start and end is in different bytes
        // e.g. 0b0000xxxx|xxxx0000
        let desired_start_pos: u8 = start_remainder;

        // 1. Set start
        // We know end is in a different byte, so we fill start byte, starting at desired pos
        let start_bits_to_set: u8 = 8 - desired_start_pos;
        // println!("start_bits_to_set: {}", start_bits_to_set);

        // Nasty casts because we might do 1 << 8 (which is 256 that can't fit in u8)
        // so treat the cast as a usize, but after subtracting -1 we're in the safe range for u8
        // again
        let mut bits_to_set_as_bits: u8 = ((1 << start_bits_to_set) as usize - 1) as u8;
        // println!("Setting start byte '{}'", start_byte);
        if used {
            self.set_bitmap_or(start_byte as isize, bits_to_set_as_bits);
        } else {
            self.set_bitmap_and(start_byte as isize, !bits_to_set_as_bits);
        }

        let mut bits_left_to_set = count - (start_bits_to_set as usize);

        // 2. Set end (if not full)
        if last_remainder > 0 {
            // println!("Setting last byte '{}'", last_byte);
            // We're only setting a specific amount of bits in the last byte
            let desired_end_pos = last_remainder;
            let bits_in_last_byte_to_set = last_remainder;
            bits_to_set_as_bits = ((1 << bits_in_last_byte_to_set as usize) - 1) as u8;

            if used {
                self.set_bitmap_or(
                    last_byte as isize,
                    bits_to_set_as_bits << (8 - desired_end_pos),
                );
            } else {
                self.set_bitmap_and(
                    last_byte as isize,
                    !(bits_to_set_as_bits << (8 - desired_end_pos)),
                );
            }

            bits_left_to_set -= bits_in_last_byte_to_set as usize;
        }

        if bits_left_to_set == 0 {
            // println!("start+end did the trick, no more to do");
            return;
        }

        let num_bytes_spanned = bits_left_to_set / 8;

        // start byte has already been set, and last byte has been set if it doesn't fill entire byte
        let last_byte_to_set = start_byte + 1 + num_bytes_spanned;

        let mut i = start_byte + 1;
        while i < last_byte_to_set {
            self.set_bitmap(i as isize, if used { 0xFF } else { 0 });

            i += 1;
        }
    }

    fn is_slice_aligned(&self, slice: usize, alignment: u8) -> bool {
        let start_offset = slice * (BYTES_PER_SLICE as usize);
        let base_addr = self.memory_addr as usize;

        // We will prepend 4 bytes to the address before it is returned
        // Make sure that the *final* pointer is aligned
        let pointer_addr = base_addr + start_offset + 4;

        return (pointer_addr % (alignment as usize)) == 0;
    }

    fn get_bitmap_byte(&self, offset: isize) -> u8 {
        unsafe { *self.bitmap_addr.offset(offset) }
    }

    fn set_bitmap(&self, offset: isize, value: u8) {
        unsafe {
            *self.bitmap_addr.offset(offset) = value;
        }
    }

    fn set_bitmap_or(&self, offset: isize, value: u8) {
        unsafe {
            let existing = self.get_bitmap_byte(offset);
            *self.bitmap_addr.offset(offset) = existing | value;
        }
    }

    fn set_bitmap_and(&self, offset: isize, value: u8) {
        unsafe {
            let existing = self.get_bitmap_byte(offset);
            *self.bitmap_addr.offset(offset) = existing & value;
        }
    }
}
