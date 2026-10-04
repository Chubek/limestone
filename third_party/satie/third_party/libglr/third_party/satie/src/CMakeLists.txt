set(SATIE_SOURCES
  Common.cpp Satie.cpp SatieADT.cpp SatieAutomata.cpp SatieBag.cpp SatieBV.cpp
  SatieCDCL.cpp SatieDPLL.cpp SatieEUF.cpp SatieFP.cpp SatieIDL.cpp SatieISL.cpp
  SatieLIA.cpp SatieLP.cpp SatieMILP.cpp SatieModule.cpp SatieNative.cpp SatieNRA.cpp
  SatieODE.cpp SatiePlugin.cpp SatiePoly.cpp SatieQuant.cpp SatieSAT.cpp
  SatieSequence.cpp SatieSet.cpp SatieSMT.cpp SatieSymSolve.cpp
  SatieFrontendCommon.cpp SatieFrontendCNF.cpp SatieFrontendWCNF.cpp
  SatieFrontendOPB.cpp SatieFrontendSMTLIB2.cpp SatieFrontendIPASIR.cpp
  SatieFrontendSatieLisp.cpp)
target_sources(satie PRIVATE ${SATIE_SOURCES})
