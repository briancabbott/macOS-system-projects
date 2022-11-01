; Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
;
; @APPLE_LICENSE_HEADER_START@
; 
; Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
; Reserved.  This file contains Original Code and/or Modifications of
; Original Code as defined in and that are subject to the Apple Public
; Source License Version 2.0 (the "License").  You may not use this file
; except in compliance with the License.  Please obtain a copy of the
; License at http://www.apple.com/publicsource and read it before using
; this file.
; 
; The Original Code and all software distributed under the License are
; distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
; EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
; INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
; License for the specific language governing rights and limitations
; under the License.
; 
; @APPLE_LICENSE_HEADER_END@
;
; Boot Loader: boot0
;
; A small boot sector program written in x86 assembly whose only
; responsibility is to load the booter into memory
; and jump to the booter's entry point.
; The booter partition can be a primary or a logical partition.
; 
; This boot loader can be placed at any of the following places:
; 1. Boot sector of an extended partition
; 2. Boot sector of a primary partition
; 3. Boot sector of a logical partition
;
; It expects that the MBR has left the drive number in DL
; and a pointer to the partition entry in SI.
;
; In order to coexist with a fdisk partition table (64 bytes), and
; leave room for a two byte signature (0xAA55) in the end, boot0 is
; restricted to 446 bytes (512 - 64 - 2). If boot0 did not have to
; live in the MBR, then we would have 510 bytes to work with.
;
; boot0 is always loaded by the BIOS or another booter to 0:7C00h.
;
; This code is written for the NASM assembler.
;   nasm boot0.s -o boot0


;
; Set to 1 to enable obscure debug messages.
;
DEBUG                EQU  0

;
; Set to 1 to support loading the booter (boot2) from a
; logical partition.
;
EXT_PART_SUPPORT     EQU  1

;
; Various constants.
;
kBoot0Segment        EQU  0x0000
kBoot0Stack          EQU  0xFFF0        ; boot0 stack pointer
kBoot0LoadAddr       EQU  0x7C00        ; boot0 load address
kBoot0RelocAddr      EQU  0xE000        ; boot0 relocated address

kMBRBuffer           EQU  0x1000        ; MBR buffer address
kExtBuffer           EQU  0x1200        ; EXT boot block buffer address

kPartTableOffset     EQU  0x1be
kMBRPartTable        EQU  kMBRBuffer + kPartTableOffset
kExtPartTable        EQU  kExtBuffer + kPartTableOffset

kBoot1uSectors       EQU  16            ; sectors to load for boot2
kBoot1uAddress       EQU  0x0000        ; boot2 load address
kBoot1uSegment       EQU  0x1000        ; boot2 load segment
	
kSectorBytes         EQU  512           ; sector size in bytes
kBootSignature       EQU  0xAA55        ; boot sector signature

kPartCount           EQU  4             ; number of paritions per table
kPartTypeBoot        EQU  0xab          ; boot2 partition type
kPartTypeUFS         EQU  0xa8          ; 
kPartTypeExtDOS      EQU  0x05          ; DOS extended partition type
kPartTypeExtWin      EQU  0x0f          ; Windows extended partition type
kPartTypeExtLinux    EQU  0x85          ; Linux extended partition type
kPartActive          EQU  0x80

%ifdef FLOPPY
kDriveNumber         EQU  0x00
%else
kDriveNumber         EQU  0x80
%endif

;
; Format of fdisk partition entry.
;
; The symbol 'part_size' is automatically defined as an `EQU'
; giving the size of the structure.
;
           struc part
.bootid:   resb 1      ; bootable or not 
.head:     resb 1      ; starting head, sector, cylinder
.sect:     resb 1      ;
.cyl:      resb 1      ;
.type:     resb 1      ; partition type
.endhead   resb 1      ; ending head, sector, cylinder
.endsect:  resb 1      ;
.endcyl:   resb 1      ;
.lba:      resd 1      ; starting lba
.sectors   resd 1      ; size in sectors
           endstruc

;
; Macros.
;
%macro DebugCharMacro 1
    mov   al, %1
    call  print_char
%endmacro

%if DEBUG
%define DebugChar(x)  DebugCharMacro x
%else
%define DebugChar(x)
%endif

;--------------------------------------------------------------------------
; Start of text segment.

    SEGMENT .text

    ORG     0x7C00

;--------------------------------------------------------------------------
; Boot code is loaded at 0:7C00h.
;
start
    DebugChar('!')
%if DEBUG
    mov     al, dl
    call    print_hex
%endif
    ;
    ; Set up the stack to grow down from kBoot0Segment:kBoot0Stack.
    ; Interrupts should be off while the stack is being manipulated.
    ;
    cli                             ; interrupts off
    xor     ax, ax                  ; zero ax
    mov     ss, ax                  ; ss <- 0
    mov     sp, kBoot0Stack         ; sp <- top of stack
    sti                             ; reenable interrupts

    mov     es, ax                  ; es <- 0
    mov     ds, ax                  ; ds <- 0

%if 0
    ; 
    ; Save SI register.
    ;
    mov     bx, si

    ;
    ; Relocate ourselves.
    ;
    mov     si, kBoot0LoadAddr      ; si <- source
    mov     di, kBoot0RelocAddr     ; di <- destination
    ;
    cld                             ; auto-increment SI and/or DI registers
    mov     cx, kSectorBytes/2      ; copy 256 words
    repnz   movsw                   ; repeat string move (word) operation

    ; Code relocated, jump to start_reloc in relocated location.
    ;
    jmp     0:start_reloc

;--------------------------------------------------------------------------
; Start execution from the relocated location.
;
start_reloc:
%endif
        
    DebugChar('*')

.loop:

%if DEBUG
    mov     al, dl
    call    print_hex
%endif

    ;
    ; Clear various flags in memory.
    ;
    xor     eax, eax
    mov     [ebios_lba], eax        ; clear EBIOS LBA offset
    mov     [ebios_present], al     ; clear EBIOS support flag

    ;
    ; Check if EBIOS is supported for this hard drive.
    ;
    mov     ah, 0x41                ; Function 0x41
    mov     bx, 0x55AA              ; check signature
;   mov     dl, kDriveNumber        ; Drive number
    int     0x13

    ;
    ; If successful, the return values are as follows:
    ;
    ; carry = 0
    ; ah    = major version of EBIOS extensions (0x21 = version 1.1)
    ; al    = altered
    ; bx    = 0xAA55
    ; cx    = support bits. bit 0 must be set for function 0x42.
    ;
    jc      .ebios_check_done
    cmp     bx, 0xAA55              ; check BX = 0xAA55
    jnz     .ebios_check_done
    test    cl, 0x01                ; check enhanced drive read support
    setnz   [ebios_present]         ; EBIOS supported, set flag
    DebugChar('E')                  ; EBIOS supported
.ebios_check_done:

    DebugChar('L')

%if DEBUG
    mov     al, BYTE [si + part.type]
    call    print_hex
%endif
        
    ; Check to make sure our partition is the correct type.
    cmp     BYTE [si + part.type], kPartTypeUFS
    je      load_boot

part_error:
    ; Drat, the partition entry was the wrong type.
    ; Print an error and hang.
    mov     si, part_error_str
    call    print_string

hang:
    hlt
    jmp     SHORT hang

    ;--------------------------------------------------------------------------
    ; Load the booter from the partition.

load_boot:

    ;
    ; Found boot partition, read boot1u image to memory.
    ;
    mov     al, kBoot1uSectors
    mov     bx, kBoot1uSegment
    mov     es, bx
    mov     bx, kBoot1uAddress
    call    load                    ;
    jc      part_error              ; load error, keep looking?

    DebugChar('^')

    ;
    ; Jump to boot1u. The drive number is already in register DL.
    ;
    ; The first sector loaded from the disk is reserved for the boot
    ; block (boot1), adjust the jump location by adding a sector offset.
    ;
    jmp     kBoot1uSegment:kBoot1uAddress + kSectorBytes

.exit:
    ;
    ; Boot partition not found. Giving up.
    ;
    pop     si
    pop     cx
    ret

;--------------------------------------------------------------------------
; load - Load one or more sectors from a partition.
;
; Arguments:
;   AL = number of 512-byte sectors to read.
;   ES:BX = pointer to where the sectors should be stored.
;   DL = drive number (0x80 + unit number)
;   SI = pointer to the partition entry.
;
; Returns:
;   CF = 0 success
;        1 error
;
load:
    push    cx
    test    BYTE [ebios_present], 1
    jz      .chs

.ebios:
    mov     cx, 5                   ; load retry count
.ebios_loop:
    call    read_lba                ; use INT13/F42
    jnc     .exit
    loop    .ebios_loop

.chs:
    mov     cx, 5                   ; load retry count
.chs_loop:
    call    read_chs                ; use INT13/F2
    jnc     .exit
    loop    .chs_loop

.exit
    pop     cx
    ret

;--------------------------------------------------------------------------
; read_chs - Read sectors from a partition using CHS addressing.
;
; Arguments:
;   AL = number of 512-byte sectors to read.
;   ES:BX = pointer to where the sectors should be stored.
;   DL = drive number (0x80 + unit number)
;   SI = pointer to the partition entry.
;
; Returns:
;   CF = 0 success
;        1 error
;
read_chs:
    pusha                           ; save all registers

    ;
    ; Read the CHS start values from the partition entry.
    ;
    mov     dh, [ si + part.head ]  ; drive head
    mov     cx, [ si + part.sect ]  ; drive sector + cylinder

    ;
    ; INT13 Func 2 - Read Disk Sectors
    ;
    ; Arguments:
    ;   AH    = 2
    ;   AL    = number of sectors to read
    ;   CH    = lowest 8 bits of the 10-bit cylinder number
    ;   CL    = bits 6 & 7: cylinder number bits 8 and 9
    ;           bits 0 - 5: starting sector number (1-63)
    ;   DH    = starting head number (0 to 255)
    ;   DL    = drive number (80h + drive unit)
    ;   es:bx = pointer where to place sectors read from disk
    ;
    ; Returns:
    ;   AH    = return status (sucess is 0)
    ;   AL    = burst error length if ah=0x11 (ECC corrected)
    ;   carry = 0 success
    ;           1 error
    ;
;   mov     dl, kDriveNumber
    mov     ah, 0x02                ; Func 2
    int     0x13                    ; INT 13
    jnc     .exit

    DebugChar('r')                  ; indicate INT13/F2 error

    ;
    ; Issue a disk reset on error.
    ; Should this be changed to Func 0xD to skip the diskette controller
    ; reset?
    ;
    xor     ax, ax                  ; Func 0
    int     0x13                    ; INT 13
    stc                             ; set carry to indicate error

.exit:
    popa
    ret

;--------------------------------------------------------------------------
; read_lba - Read sectors from a partition using LBA addressing.
;
; Arguments:
;   AL = number of 512-byte sectors to read (valid from 1-127).
;   ES:BX = pointer to where the sectors should be stored.
;   DL = drive number (0x80 + unit number)
;   SI = pointer to the partition entry.
;
; Returns:
;   CF = 0 success
;        1 error
;
read_lba:
    pusha                           ; save all registers
    mov     bp, sp                  ; save current SP

    ;
    ; Create the Disk Address Packet structure for the
    ; INT13/F42 (Extended Read Sectors) on the stack.
    ;

    ; push    DWORD 0               ; offset 12, upper 32-bit LBA
    push    ds                      ; For sake of saving memory,
    push    ds                      ; push DS register, which is 0.

    mov     ecx, [ebios_lba]        ; offset 8, lower 32-bit LBA
    add     ecx, [si + part.lba]
    push    ecx

    push    es                      ; offset 6, memory segment

    push    bx                      ; offset 4, memory offset

    xor     ah, ah                  ; offset 3, must be 0
    push    ax                      ; offset 2, number of sectors

    push    WORD 16                 ; offset 0-1, packet size

    ;
    ; INT13 Func 42 - Extended Read Sectors
    ;
    ; Arguments:
    ;   AH    = 0x42
    ;   DL    = drive number (80h + drive unit)
    ;   DS:SI = pointer to Disk Address Packet
    ;
    ; Returns:
    ;   AH    = return status (sucess is 0)
    ;   carry = 0 success
    ;           1 error
    ;
    ; Packet offset 2 indicates the number of sectors read
    ; successfully.
    ;
;   mov     dl, kDriveNumber
    mov     si, sp
    mov     ah, 0x42
    int     0x13

    jnc     .exit

    DebugChar('R')                  ; indicate INT13/F42 error

    ;
    ; Issue a disk reset on error.
    ; Should this be changed to Func 0xD to skip the diskette controller
    ; reset?
    ;
    xor     ax, ax                  ; Func 0
    int     0x13                    ; INT 13
    stc                             ; set carry to indicate error

.exit:
    mov     sp, bp                  ; restore SP
    popa
    ret

;--------------------------------------------------------------------------
; Write a string to the console.
;
; Arguments:
;   DS:SI   pointer to a NULL terminated string.
;
; Clobber list:
;   AX, BX, SI
;
print_string
    mov     bx, 1                   ; BH=0, BL=1 (blue)
    cld                             ; increment SI after each lodsb call
.loop
    lodsb                           ; load a byte from DS:SI into AL
    cmp     al, 0                   ; Is it a NULL?
    je      .exit                   ; yes, all done
    mov     ah, 0xE                 ; INT10 Func 0xE
    int     0x10                    ; display byte in tty mode
    jmp     short .loop
.exit
    ret

%if DEBUG

;--------------------------------------------------------------------------
; Write a ASCII character to the console.
;
; Arguments:
;   AL = ASCII character.
;
print_char
    pusha
    mov     bx, 1                   ; BH=0, BL=1 (blue)
    mov     ah, 0x0e                ; bios INT 10, Function 0xE
    int     0x10                    ; display byte in tty mode
    popa
    ret

;--------------------------------------------------------------------------
; Write the byte value to the console in hex.
;
; Arguments:
;   AL = Value to be displayed in hex.
;
print_hex:
    push    ax
    ror     al, 4
    call    print_nibble            ; display upper nibble
    pop     ax
    call    print_nibble            ; display lower nibble

    mov     al, 10                  ; carriage return
    call    print_char
    mov     al, 13
    call    print_char
    ret

print_nibble:
    and     al, 0x0f
    add     al, '0'
    cmp     al, '9'
    jna     .print_ascii
    add     al, 'A' - '9' - 1
.print_ascii:
    call    print_char
    ret

%endif ; DEBUG

;--------------------------------------------------------------------------
; NULL terminated strings.
;
part_error_str   db  10, 13, 'Error loading UFS partition', 0

;--------------------------------------------------------------------------
; Pad the rest of the 512 byte sized booter with zeroes. The last
; two bytes is the mandatory boot sector signature.
;
; If the booter code becomes too large, then nasm will complain
; that the 'times' argument is negative.

;;pad_boot
 ;;   times 446-($-$$) db 0

%ifdef FLOPPY
;--------------------------------------------------------------------------
; Put fake partition entries for the bootable floppy image
;
part1bootid     db    0x80          ; first partition active
part1head       db    0x00          ; head #
part1sect       db    0x02          ; sector # (low 6 bits)
part1cyl        db    0x00          ; cylinder # (+ high 2 bits of above)
part1systid     db    0xab          ; Apple boot partition
times   3       db    0x00          ; ignore head/cyl/sect #'s
part1relsect    dd    0x00000001    ; start at sector 1
part1numsect    dd    0x00000080    ; 64K for booter
part2bootid     db    0x00          ; not active
times   3       db    0x00          ; ignore head/cyl/sect #'s
part2systid     db    0xa8          ; Apple UFS partition
times   3       db    0x00          ; ignore head/cyl/sect #'s
part2relsect    dd    0x00000082    ; start after booter
; part2numsect  dd    0x00000abe    ; 1.44MB - 65K
part2numsect    dd    0x000015fe    ; 2.88MB - 65K
%endif

pad_table_and_sig
    times 510-($-$$) db 0
    dw    kBootSignature

	ABSOLUTE 0xE400

;
; In memory variables.
;
ebios_lba       resd  1   ; starting LBA of the intial extended partition.
ebios_present   resb  1   ; 1 if EBIOS is supported, 0 otherwise.

    END
