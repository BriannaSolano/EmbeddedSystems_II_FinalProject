# EmbeddedSystems_II_FinalProject

Final Project for Embbedded Systems II Rutgers Fall 2021
---------------------------------------------------------

Description 
-----------------------------------------------------------

Using a Zybo board, RTCC PMOD, TMP3 PMOD, and DHB1 PMOD to create an adaptive sprinkler system. The DHB1 PMOD powers the motors on 3 times a day based on a given time. Time and Date are given by the DHB1 PMOD. When it is time to run the motors which simulate spinklers, the Zybo board checks the time, given by the TMP3 PMOD and this determines how long the motors run. If the motors run normally at every given time stamp, but if detected that the temperture is too high, the motors would run for a longer duration. 

Material
----------------------------------------------------------
1x Zybo board

1x RTCC PMOD

1x TMP3 PMOD

1x DHB1 PMOD

1x DC motor
