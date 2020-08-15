OliThink5 (c) Oliver Brausch 11.Apr.2008, ob112@web.de, http://home.arcor.de/dreamlike

Version: 5.1.3
Protocol: Winboard 2
HashSize: 128mb
Ponder: Yes
NullMove: Yes
OpeningBook: Small
EndgameTables: No
AnalyzeMode: No
Evaluation: Mobility Only
LinesOfCode: 1542
Stability: 100%
Strength: 200 ELO less than Glaurung/Crafty

Create/Change book: 
- The file must be named "olibook.pgn".
- The String "[Black: OliThink" indicates that the engine can play this opening if black.
- Only the first line of the opening will be read and the line must end with "*"

Changes since 5.1.2:
Support for opening books, including a small one
Parsing of PGN Moves
Minor restructure in quiesce, thus increasing strength

V5.1.2: changes since 5.1.1:
PV-Search, getting rid of Alpha-Beta Windows
Minor changes in time control
Draw Check for insufficient Material
Pruning Window in Quiescence reduced
Search extension moved into the move-loop