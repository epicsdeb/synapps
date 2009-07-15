## devScaler059Init(port,base,inst,rate)
#     port - Asyn port driver name
#     base - Base address
#     inst - Board instance
#     rate - Poll frequency (in Hz)
#     intr - Enable interrupt processing
#devAsynScaler059Init("SCALER1",0x390,0,20,1)
#dbLoadRecords("../../db/scaler.db","P=ebrick:,S=scaler1,DTYP=devAsynScaler059,FREQ=20000000,OUT=@asyn(SCALER1 0)")

## devScaler071Init(port,rate)
#     port - Asyn port driver name
#     rate - Poll frequency (in Hz)
#devAsynScaler071Init("SCALER1",20)
#dbLoadRecords("../../db/scaler32.db","P=ebrick:,S=scaler1,DTYP=devAsynScaler071,FREQ=50000000,OUT=@asyn(SCALER1 0)")
