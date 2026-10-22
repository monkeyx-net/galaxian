# Shared by the MAME tests: the options. Needs MAME, ROMPATH (the dir holding
# galaxian.zip or galaxian/) and WORKDIR. Galaxian has no NVRAM: every run
# starts from power-on.
file(MAKE_DIRECTORY "${WORKDIR}")
# MAME keeps the DIP switches it was last given in its cfg folder (the
# cocktail run sets the Cabinet switch): every test starts without one
file(REMOVE_RECURSE "${WORKDIR}/cfg")
set(mame_args galaxian -rompath "${ROMPATH}" -cfg_directory "${WORKDIR}/cfg" -nvram_directory "${WORKDIR}/nvram"
    -snapshot_directory "${WORKDIR}/snap" -snapview native
    # every frame rendered, so a snapshot shows the frame it is taken in
    -video none -sound none -nothrottle -frameskip 0 -noplugin data -skip_gameinfo)
