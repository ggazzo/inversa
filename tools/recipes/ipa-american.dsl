# IPA Americana — single infusion + 60min boil
# 5L batch, simple grist (90% Pilsen + 10% Crystal 60L)
# Bittering: Magnum @ 60 · Flavor: Cascade @ 15 · Aroma: Citra @ 5 · Whirlpool: Mosaic

STEP "Mostura 67°C"
SET_TEMP 67
WAIT_TEMP 1.0
WAIT_TIMER 60

STEP "Mash-out 76°C"
MASH_OUT 76
WAIT_TIMER 10

STEP "Aquecer para fervura"
SET_TEMP 100
WAIT_BOIL

STEP "Fervura 60 min"
BOIL 60
ADD_HOP 60 "Magnum"
ADD_HOP 15 "Cascade"
ADD_HOP 5  "Citra"
ADD_HOP 0  "Mosaic"

HEATER_OFF
WAIT_CONFIRM "Resfriar wort para 20°C e transferir para fermentador"
