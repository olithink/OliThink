OliThink5 (c) Oliver Brausch 27.Oct.2008, ob112@web.de, http://home.arcor.de/dreamlike

Version: 5.1.7
Protocol: Winboard 2
HashSize: 128mb
Ponder: Yes
OpeningBook: Small
EndgameTables: No
AnalyzeMode: No
SearchMethods: Nullmove, Internal Iterative Deepening, Check Extension
Evaluation: Just mobility and a very simple pawnprogressing/pawnhanging evaluation
LinesOfCode: 1562
Stability: 100%
Strength: slightly weaker than Glaurung/Crafty

v5.1.7: changes since 5.1.6:
Knight mobility evaluation reduced
Constant for "draw for insufficiant matierial" reduced 
Minor infrastructual changes and little bugs removed (setboard, moveparser)

v5.1.6: changes since 5.1.5:
Rewrite of the pawn-progress evalution. It was a factor 2 too high. This made the engine stronger

v5.1.5: changes since 5.1.4:
Evaluation consideres the fact that with just one minor piece it can't win
Small changes in punishing hanging pawns (see 5.1.4)

v5.1.4: changes since 5.1.3:
Fixed minor bug that position setup by GUI interfered with opening book
Added strategical evaluation: Free pawns are rewarded, hanging pawns punished

V5.1.3: changes since 5.1.2:
Support for opening books, including a small one
Parsing of PGN Moves
Minor restructure in quiesce, thus increasing strength

V5.1.2: changes since 5.1.1:
PV-Search, getting rid of Alpha-Beta Windows
Minor changes in time control
Draw Check for insufficient Material
Pruning Window in Quiescence reduced
Search extension moved into the move-loop

Create/Change book: 
- The file must be named "olibook.pgn".
- The Substring "[Black: OliThink" indicates that the engine can play this opening if black.
- Only the first line of the opening will be read and the line must end with "*"
