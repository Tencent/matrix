# Unwinder Support Per Android Release
This document describes the changes in the way the libunwindstack
unwinder works on different Android versions. It does not describe
every change in the code made between different versions, but is
meant to allow an app developer to know what might be supported
on different versions. It also describes the different way an unwind
will display on different versions of Android.

## Android P
libunwindstack was first introduced in Android P.

* Supports up to and including Dwarf 4 unwinding information.
  See http://dwarfstd.org/ for Dwarf standards.
* Supports Arm exidx unwinding.
* Supports the gdb JIT unwinding interface, which is how ART creates unwinding
  information for the JIT'd Java frames.
* Supports special frames added to represent an ART Java interpreter frame.
  ART has marked the dex pc using cfi information that the unwinder
  understands and handles by adding a new frame in the stacktrace.

## Note
By default, lld creates two separate maps of the elf in memory, one read-only
and one read/executable. The libunwindstack on P and the unwinder on older
versions of Android will not unwind properly in this case. For apps that
target Android P or older, make sure that `-Wl,--no-rosegment` is
included in linker arguments when using lld.

## Android Q
* Fix bug (b/109824792) that handled load bias data incorrectly when
  FDEs use pc relative addressing in the eh\_frame\_hdr.
  Unfortunately, this wasn't fixed correctly in Q since it assumes
  that the bias is coming from the program header for the executable
  load. The real fix was to use the bias from the actual section data and
  is not completely fixed until Android R. For apps targeting Android Q,
  if it is being compiled with the llvm linker lld, it might be necessary
  to add the linker option `-Wl,-zseparate-code` to avoid creating an elf
  created this way.
* Change the way the exidx section offset is found (b/110704153). Before
  the p\_vaddr value from the program header minus the load bias was used
  to find the start of the exidx data. Changed to use the p\_offset since
  it doesn't require any load bias manipulations.
* Fix bug handling of dwarf sections without any header (b/110235461).
  Previously, the code assumed that FDEs are non-overlapping, and the FDEs
  are always in sorted order from low pc to high pc. Thus the code would
  read the entire set of CIEs/FDEs and then do a binary search to find
  the appropriate FDE for a given pc. Now the code does a sequential read
  and stops when it finds the FDE for a pc. It also understands the
  overlapping FDEs, so find the first FDE that matches a pc. In practice,
  elf files with this format only ever occurs if the file was generated
  without an eh\_frame/eh\_frame\_hdr section and only a debug\_frame. The
  other way this has been observed is when running simpleperf to unwind since
  sometimes there is not enough information in the eh\_frame for all points
  in the executable. On Android P, this would result in some incorrect
  unwinds coming from simpleperf. Nearly all crashes from Android P should
  be correct since the eh\_frame information was enough to do the unwind
  properly.
* Be permissive of badly formed elf files. Previously, any detected error
  would result in unwinds stopping even if there is enough valid information
  to do an unwind.
  * The code now allows program header/section header offsets to point
    to unreadable memory. As long as the code can find the unwind tables,
    that is good enough.
  * The code allows program headers/section headers to be missing.
  * Allow a symbol table section header to point to invalid symbol table
    values.
* Support for the linker read-only segment option (b/109657296).
  This is a feature of lld whereby there are two sections that
  contain elf data. The first is read-only and contains the elf header data,
  and the second is read-execute or execute only that
  contains the executable code from the elf. Before this, the unwinder
  always assumed that there was only a single read-execute section that
  contained the elf header data and the executable code.
* Build ID information for elf objects added. This will display the
  NT\_GNU\_BUILD\_ID note found in elf files. This information can be used
  to identify the exact version of a shared library to help get symbol
  information when looking at a crash.
* Add support for displaying the soname from an apk frame. Previously,
  a frame map name would be only the apk, but now if the shared library
  in the apk has set a soname, the map name will be `app.apk!libexample.so`
  instead of only `app.apk`.
* Minimal support for Dwarf 5. This merely treats a Dwarf 5 version
  elf file as Dwarf 4. It does not support the new dwarf ops in Dwarf 5.
  Since the new ops are not likely to be used very often, this allows
  continuing to unwind even when encountering Dwarf 5 elf files.
* Fix bug in pc handling of signal frames (b/130302288). In the previous
  version, the pc would be wrong in the signal frame. The rest of the
  unwind was correct, only the frame in the signal handler was incorrect
  in Android P.
* Detect when an elf file is not readable so that a message can be
  displayed indicating that. This can happen when an app puts the shared
  libraries in non-standard locations that are not readable due to
  security restrictions (selinux rules).

## Android R
* Display the offsets for Java interpreter frames. If this frame came
  from a non-zero offset map, no offset is printed. Previously, the
  line would look like:

    #17 pc 00500d7a  GoogleCamera.apk (com.google.camera.AndroidPriorityThread.run+10)

  to:

    #17 pc 00500d7a  GoogleCamera.apk (offset 0x11d0000) (com.google.camera.AndroidPriorityThread.run+10)
* Fix bug where the load bias was set from the first PT\_LOAD program
  header that has a zero p\_offset value. Now it is set from the first
  executable PT\_LOAD program header. This has only ever been a problem
  for host executables compiled for the x86\_64 architecture.
* Switched to the libc++ demangler for function names. Previously, the
  demangler used was not complete, so some less common demangled function
  names would not be properly demangled or the function name would not be
  demangled at all.
* Fix bug in load bias handling. If the unwind information in the eh\_frame
  or eh\_frame\_hdr does not have the same bias as the executable section,
  and uses pc relative FDEs, the unwind will be incorrect. This tends
  to truncate unwinds since the unwinder could not find the correct unwind
  information for a given pc.
