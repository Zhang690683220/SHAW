include(FindUnixCommands)

# remove possibly existing snapshots
execute_process(COMMAND ${BASH} -c "rm -rf snaps_vp_0 snaps_sp_0")
execute_process(COMMAND ${BASH} -c "rm -rf snaps_vp_1 snaps_sp_1")
execute_process(COMMAND ${BASH} -c "rm -rf snaps_vp_2 snaps_sp_2")

# first run the exe
execute_process(COMMAND ${CMD_FOM} ${INPUT_FNAME} RESULT_VARIABLE CMD_RESULT)
message(${CMD_RESULT})
if(RES)
  message(FATAL_ERROR "Fom run failed")
endif()

execute_process(COMMAND ${CMD_COMPARE} snaps_vp_0 snaps_vp_gold_0 1e-13 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for vp_0 is not clean")
endif()
execute_process(COMMAND ${CMD_COMPARE} snaps_sp_0 snaps_sp_gold_0 1e-10 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for sp_0 is not clean")
endif()

execute_process(COMMAND ${CMD_COMPARE} snaps_vp_1 snaps_vp_gold_1 1e-13 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for vp_1 is not clean")
endif()
execute_process(COMMAND ${CMD_COMPARE} snaps_sp_1 snaps_sp_gold_1 1e-10 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for sp_1 is not clean")
endif()

execute_process(COMMAND ${CMD_COMPARE} snaps_vp_2 snaps_vp_gold_2 1e-13 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for vp_2 is not clean")
endif()
execute_process(COMMAND ${CMD_COMPARE} snaps_sp_2 snaps_sp_gold_2 1e-10 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for sp_2 is not clean")
endif()

execute_process(COMMAND ${CMD_COMPARE} snaps_vp_3 snaps_vp_gold_3 1e-13 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for vp_3 is not clean")
endif()
execute_process(COMMAND ${CMD_COMPARE} snaps_sp_3 snaps_sp_gold_3 1e-10 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for sp_3 is not clean")
endif()

execute_process(COMMAND ${CMD_COMPARE} snaps_vp_4 snaps_vp_gold_4 1e-13 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for vp_4 is not clean")
endif()
execute_process(COMMAND ${CMD_COMPARE} snaps_sp_4 snaps_sp_gold_4 1e-10 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for sp_4 is not clean")
endif()

execute_process(COMMAND ${CMD_COMPARE} snaps_vp_5 snaps_vp_gold_5 1e-13 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for vp_5 is not clean")
endif()
execute_process(COMMAND ${CMD_COMPARE} snaps_sp_5 snaps_sp_gold_5 1e-10 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for sp_5 is not clean")
endif()

execute_process(COMMAND ${CMD_COMPARE} snaps_vp_6 snaps_vp_gold_6 1e-13 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for vp_6 is not clean")
endif()
execute_process(COMMAND ${CMD_COMPARE} snaps_sp_6 snaps_sp_gold_6 1e-10 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for sp_6 is not clean")
endif()

execute_process(COMMAND ${CMD_COMPARE} snaps_vp_7 snaps_vp_gold_7 1e-13 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for vp_7 is not clean")
endif()
execute_process(COMMAND ${CMD_COMPARE} snaps_sp_7 snaps_sp_gold_7 1e-10 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for sp_7 is not clean")
endif()

execute_process(COMMAND ${CMD_COMPARE} snaps_vp_8 snaps_vp_gold_8 1e-13 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for vp_8 is not clean")
endif()
execute_process(COMMAND ${CMD_COMPARE} snaps_sp_8 snaps_sp_gold_8 1e-10 RESULT_VARIABLE RES)
if(RES)
  message(FATAL_ERROR "Diff for sp_8 is not clean")
endif()
