# runtime2005/to_asm6f.sed -- translate blargg's 2005-era assembler dialect
# (the custom devcart assembler the pal_apu_tests / blargg_apu_2005 /
# sprite_hit_tests sources were written for) into asm6f. Run with GNU
# `sed -E -f to_asm6f.sed upstream.a > staged.a`; every rule is purely
# textual and deterministic, and each exists because asm6f rejects or
# misreads the original construct:
#
# 1. `.code` is a section marker with no asm6f equivalent (everything is
#    one code stream here): drop it, keep any trailing comment.
s/^[[:space:]]*\.code[[:space:]]*(;.*)?$/\1/
#
# 2. A line starting with `:` is an anonymous label that `-` (nearest
#    previous) and `+` (nearest next) refer to. asm6f's anonymous labels are
#    directional (a `-` label only answers `-` references, `+` only `+`), so
#    each `:` becomes both, at the same address, followed by its statement.
s/^:([[:space:]]+)(.*)$/-\n+\n\t\2/
#
# 3. `error` is an asm6f directive (ERROR), so it cannot be a label: rename
#    the definition and a branch/jump operand that names it.
s/^error:/veloce_error:/
s/([[:space:]])error([[:space:]]*(;.*)?)$/\1veloce_error\2/
