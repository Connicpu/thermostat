import pcbnew, os, glob
E='/workspace/ez/lib.pretty'; OUT='kicad/tstat.pretty'
mm=pcbnew.FromMM
M={ 'ESP32-C6-WROOM-1':'WIRELM-SMD_ESP32-C6-WROOM-1','MOC3063S':'SMD-6_L7.3-W6.5-P2.54-LS10.2-BL',
 'HVAC_TERM_15EDGRC':'CONN-TH_5P-P3.50_MX15EDGRC-3.5-05P-GN01-CU-A','TO-252_BT136S':'TO-252-2_L6.6-W6.1-P4.57-LS9.9-BR-CW',
 'SOT-23-6_LMR16006':'SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BR','SOT-23-6_USBLC6':'SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL',
 'SOT-23-5_STWD100':'SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BR','SOT-323_SI1308':'SOT-323_L2.0-W1.3-P1.30-LS2.1-BR',
 'MBS_MB10S':'MBS_L4.7-W3.8-P2.40-LS7.0-TL','SMA_SS210':'SMA_L4.3-W2.6-LS5.2-RD','SMB_SMBJ40A':'SMB_L4.6-W3.6-LS5.3-RD',
 'SOD-123_B5819W':'SOD-123_L2.7-W1.6-LS3.7-RD-1','CP_10x10_RVT':'CAP-SMD_BD10.0-L10.3-W10.3-LS11.0-FD_1',
 'L_4030_SWPA':'IND-SMD_L4.0-W4.0_SLW4010S','F1812_PTC':'F1812','LED_0603_KT':'LED-SMD_L1.6-W0.8-R-RD',
 'FPC24_AFC07':'FPC-SMD_AFC07-S24FCA-00','JST_SH4_STEMMA':'CONN-SMD_4P-P1.00_XY-SM04B-SRSS-TB',
 'CP_6.3x5.4_RVT':'CAP-SMD_BD6.3-L6.6-W6.6-FD','HVAC_TERM_15EDGVC':'CONN-TH_5P-P3.50_MX15EDGV-3.5-GN01-05P-CU-A','SW_TS1187A':'SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5','SOT-23_AO3400A':'SOT-23-3_L2.9-W1.3-P1.90-LS2.4-BR'}
# copy wrl names into model path
for new,old in M.items():
    f=pcbnew.FootprintLoad(E,old); assert f,old
    f.SetFPID(pcbnew.LIB_ID('tstat',new))
    for p in f.Pads():
        n=p.GetNumber()
        if new=='ESP32-C6-WROOM-1' and (n=='' or (n.isdigit() and int(n)>=29)): p.SetNumber('29')
        if new=='FPC24_AFC07' and n in('25','26'): p.SetNumber('MP')
        if new=='SW_TS1187A': p.SetNumber({'1':'1','2':'1','3':'2','4':'2'}[n])
        if new=='JST_SH4_STEMMA' and n in('5','6'): p.SetNumber('MP')
    for m in f.Models():
        m.m_Filename='${KIPRJMOD}/tstat.3dshapes/'+os.path.basename(m.m_Filename)
    if new=='ESP32-C6-WROOM-1':
        z=pcbnew.ZONE(f); z.SetIsRuleArea(True); z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True); z.SetDoNotAllowPads(True); z.SetDoNotAllowCopperPour(True); z.SetDoNotAllowFootprints(False)
        z.SetLayerSet(pcbnew.LSET.AllCuMask()); o=z.Outline(); o.NewOutline()
        for x,y in [(-9,-15.74),(9,-15.74),(9,-9.74),(-9,-9.74)]: o.Append(mm(x),mm(y))
        z.SetZoneName('ANTENNA_KEEPOUT'); f.Add(z)
    pcbnew.FootprintSave(OUT,f)
print(sorted(os.listdir(OUT)))
