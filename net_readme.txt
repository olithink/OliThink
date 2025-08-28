


New nets - stronger play.

Datasets created using a pgn file containing entire CCRL database of engine vs engine matches.
Matches containing duplicate moves were removed using pgn-extract. Pgn download > https://drive.proton.me/urls/VMFN932P4M#V0CNXOJKgQyD

Dataset/training was done on my 12 core Xeon computer in cpu mode - 24 hours to complete 11 epoch network files.

Would be a lot faster in gpu mode if I had a compatible nvidia gpu.

Last epoch, epoch 11 should be the best.

To created new olithink.nn file using the new network epochs: 

Delete existing olithink.nn and network.txt from olithink folder.

Rename epoch file you want to use to 'network.txt'

Olithink uses the smaller quantised files - the ones with 'q' in the name.

So for example rename 'epoch-11-q.txt' to 'network.txt'

Copy file to olithink folder and run olithink engine.

Olithink will detect missing 'olithink.nn' file and create a new one from 'network.txt'

If Olithink cannot find both 'olithink.nn' and 'network.txt' it will use traditional eval instead.