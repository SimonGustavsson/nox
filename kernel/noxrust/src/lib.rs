#![feature(ptr_offset_from)]
#![no_std]

mod allocator;

use allocator::Allocator;
use core::panic::PanicInfo;

/// This function is called on panic.
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
    loop {}
}

#[no_mangle]
pub unsafe extern "C" fn rust_create_allocator(
    base_addr: *mut u8,
    max_bytes: usize,
) -> *const Allocator {
    let alloc = Allocator::new(base_addr, max_bytes);

    match alloc {
        Ok(a) => &a as *const Allocator,
        Err(_) => 0 as *const Allocator,
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_alloc(allocator: *mut Allocator, size: usize) -> *mut usize {
    match allocator.as_mut() {
        Some(a) => match a.alloc(size) {
            Ok(ptr) => ptr,
            Err(_) => 0 as *mut usize,
        },
        None => 0 as *mut usize,
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_aligned_alloc(
    allocator: *mut Allocator,
    size: usize,
    alignment: u8,
) -> *mut usize {
    match allocator.as_mut() {
        Some(a) => match a.aligned_alloc(size, alignment) {
            Ok(ptr) => ptr,
            Err(_) => 0 as *mut usize,
        },
        None => 0 as *mut usize,
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_free(allocator: *mut Allocator, ptr: *mut usize) -> bool {
    match allocator.as_mut() {
        Some(a) => a.free(ptr),
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn hello_rust(x: u32, y: u32) -> u32 {
    x + y
}
