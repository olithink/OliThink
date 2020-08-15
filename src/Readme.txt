OliThink5 (c) Oliver Brausch 23.Jun.2020, ob112@web.de, http://brausch.org

Version: 5.4.6
Protocol: Winboard 2
HashSize: 48MB
Ponder: Yes
OpeningBook: Small
EndgameTables: No
AnalyzeMode: Basic
SearchMethods: Nullmove, Internal Iterative Deepening, Check Extension, LMR
Evaluation: Just mobility and a very simple pawnprogressing evaluation
LinesOfCode: 1675
Stability: 100%
Special thanks to Dann Corbit for his support and contribution!

v5.4.7: changes since 5.4.6:
Option "-reset" instead of "-forceponder". Reinstate hanging pawn eval.

v5.4.6: changes since 5.4.5:
Adaption in printouts, protocols and engine reuse. Remove asymetrical eval.

v5.4.5: changes since 5.4.4:
Some issues about the printout of moves and evaluations have been fixed. It could fix a bug, where a strange move has been made.

v5.4.4: changes since 5.4.3:
Fixed "Bus Error 10" for plies greater than 63. Max depth increased to 64.

v5.4.3: changes since 5.4.2:
Aggressive pruning with history heuristic. Bugfix when claiming draw for insufficient material. Reads fen-string without command.

v5.4.2: changes since 5.4.1:
History heuristics increase with depth

v5.4.1: changes since 5.4.0:
Implementing a very simple Move Count Pruning

v5.4.0: changes since 5.3.5f:
Refactoring of "No mating material" which yields gains in strength

v5.3.5f: changes since 5.3.5c:
Option -forceponder. Pvlength = 128. Preparing code for corrected "No mating matieral"

v5.3.5c: changes since 5.3.5:
Reducing Hashsize again to 48MB. More flexibel reading of opening book. Selective openings for win and loss.

v5.3.5: changes since 5.3.4:
Analyze mode and Mate score.

v5.3.4: changes since 5.3.3:
Expanding HashSize to 512MB

v5.3.3: changes since 5.3.2:
Eliminating bug in getDir thanks to Francisto Modesto

v5.3.2: changes since 5.3.1:
Splitting of move generation in non-captures and captures.
Removed small bug in move-generator

v5.3.1: changes since 5.3.0:
Removed bug in quiesce

v5.3.0: changes since 5.2.9:
No LMR for passed pawns

v5.2.9: changes since 5.2.8:
More flexible reducing in Null Move Pruning

v5.2.8: changes since 5.2.7:
Removal of Singular Reply Extension
Changes in the endgame mobility evaluation

v5.2.7: changes since 5.2.6:
Added a discount for blocked pawn in the evaluation

v5.2.6: changes since 5.2.5:
Successful implementation of Late Move Reduction (LMR)

v5.2.5: changes since 5.2.4:
Storing moves history and flags for whole match
Bugfix in Internal Iterative Deepening
Bugfix when finishing matches with ponder=on

v5.2.4: changes since 5.2.3:
Pondering changed to save more time during search
Additional commands of Xboard protocol

v5.2.3: changes since 5.2.2:
At root the first move searched is from the pv and not a hash move

v5.2.2: changes since 5.2.1:
HashSize reduced from 128MB to 48MB

v5.2.1: changes since 5.2.0:
buggy search extension line removed (following the bug correction)
small bug in move-parsing corrected

v5.2.0: changes since 5.1.9:
Bug for search extensions corrected

v5.1.9: changes since 5.1.8:
Singular Reply Extension added

v5.1.8: changes since 5.1.7:
Minor change in the king mobility evaluation that has a notable effect
Opening book removed from the standard package

v5.1.7: changes since 5.1.6:
Knight mobility evaluation reduced
Constant for "draw for insufficiant material" reduced 
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
