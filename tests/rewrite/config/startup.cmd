on error break
dbLoadDatabase("dbd/snmp3Ioc.dbd")
snmp3Ioc_registerRecordDeviceDriver(pdbbase)
snmp3Load("$(CONFIG_JSON)", "$(NATIVE_PROBE)")
snmp3WorkerPath("$(WORKER_PATH)")
snmp3ConfigReport
dbLoadRecords("tests/rewrite/db/lifecycle.db", "P=Config_")
iocInit
snmp3ConfigReport
