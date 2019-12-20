# Coulomb_Counter

# @Author
Chahbaz Aman

# @Brief
A simple battery pack state of charge tracking module using current integration approach.

# @Detailed
The system samples the voltage drop across a current-shunt resistor of known resistance at 1 second intervals and interpretes the readings as current values. These are substracted from a known initial capacity of the battery (assumed 100% on reset) available in Ampere-second units.
SoC is the ratio of remaining Ampere-seconds to initial Ampere-seconds. The system stays in a idle mode when turned-off by a master control system. After a settable rest period, the systems comes on-line to get a reading of the open circuit voltage of the battery pack to estimate the initial SoC for the next run based on a log table created in the code memory from cell-testing data.

The code has been tested in the Keil uVision environment and on a prototype GCB. An ADC0808 in addition to the microcontroller.
The schematic for the GCB used for proof of concept is shared here. 
Users may implement their own signal conditioning circuitry for the voltage and current signals.
