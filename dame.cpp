#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>

#include <cstring>
#include <cmath>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <ctime>
using namespace std;

const int BOARD_SIZE = 8;
const int SQUARE_SIZE = 80;
const int BOARD_OFFSET_X = 50;
const int BOARD_OFFSET_Y = 50;
const int WINDOW_WIDTH = BOARD_OFFSET_X*2 + BOARD_SIZE * SQUARE_SIZE;
const int WINDOW_HEIGHT = BOARD_OFFSET_Y*2 + BOARD_SIZE * SQUARE_SIZE + 50;

enum Piece { 
    EMPTY = 0, 
    PLAYER1 = 1,
    PLAYER2 = 2,
    KING1   = 3,
    KING2   = 4
};

int board[BOARD_SIZE][BOARD_SIZE];

struct Move {
    int fromRow, fromCol, toRow, toCol;
    bool capture;
};

vector<Move> movesHistory;

bool vsComputer = false;
int currentPlayer = PLAYER1; // PLAYER1 va fi cel care începe

bool multiCaptureActive = false;
int activePieceRow = -1, activePieceCol = -1;

Display *display;
Window window;
GC gc;
int screen;

bool dragging = false;
int dragFromRow, dragFromCol;
int dragPiece = EMPTY;
int dragOffsetX = 0, dragOffsetY = 0;
int currentDragX = 0, currentDragY = 0;

void initBoard() {
    // Initializare tabla: toate celulele sunt EMPTY
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            board[i][j] = EMPTY;
    // Plasare piese pentru jucatorul 2 (partea de sus – randurile 0 si 1 pe patratele "maro")
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if ((i + j) % 2 == 1)
                board[i][j] = PLAYER2;
        }
    }
    // Plasare piese pentru jucatorul 1 (partea de jos – randurile 6 si 7 pe patratele "maro")
    for (int i = BOARD_SIZE - 2; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if ((i + j) % 2 == 1)
                board[i][j] = PLAYER1;
        }
    }
}

void drawBoard() {
    XClearWindow(display, window);
    // Desenam patratelele tablei
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int x = BOARD_OFFSET_X + j * SQUARE_SIZE;
            int y = BOARD_OFFSET_Y + i * SQUARE_SIZE;
            unsigned long color;
            if ((i+j) % 2 == 0) {
                color = 0xFFFFFF;
            } else {
                color = 0xCD853F;
            }
            XSetForeground(display, gc, color);
            XFillRectangle(display, window, gc, x, y, SQUARE_SIZE, SQUARE_SIZE);
        }
    }
    // Desenam piesele
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != EMPTY) {
                int x = BOARD_OFFSET_X + j * SQUARE_SIZE;
                int y = BOARD_OFFSET_Y + i * SQUARE_SIZE;
                int cx = x + SQUARE_SIZE/2;
                int cy = y + SQUARE_SIZE/2;
                int radius = SQUARE_SIZE/2 - 10;
                if (board[i][j] == PLAYER1)
                    XSetForeground(display, gc, 0xFFFFFF);
                else if (board[i][j] == PLAYER2)
                    XSetForeground(display, gc, 0x000000);
                XFillArc(display, window, gc, cx - radius, cy - radius, radius*2, radius*2, 0, 360*64);
                
                if (board[i][j] == KING1 || board[i][j] == KING2) {
                    int innerRadius = radius / 2;
                    if (board[i][j] == KING1)
                        XSetForeground(display, gc, 0xFF0000);  // rosu pentru KING1
                    else if (board[i][j] == KING2)
                        XSetForeground(display, gc, 0x0000FF);  // albastru pentru KING2
                    XFillArc(display, window, gc, cx - radius, cy - radius, radius*2, radius*2, 0, 360*64);
                }
                
            }
        }
    }
    // Daca se trage o piesa, o desenam la pozitia curenta de drag
    if (dragging) {
        int radius = SQUARE_SIZE/2 - 10;
        int cx = currentDragX;
        int cy = currentDragY;
        if (dragPiece == PLAYER1 || dragPiece == KING1)
            XSetForeground(display, gc, 0xFFFFFF);
        else if (dragPiece == PLAYER2 || dragPiece == KING2)
            XSetForeground(display, gc, 0x000000);
        XFillArc(display, window, gc, cx - radius, cy - radius, radius*2, radius*2, 0, 360*64);
        // Daca piesa in drag este dama, desenam si indiciul interior
        if (dragPiece == KING1 || dragPiece == KING2) {
            int innerRadius = radius / 2;
            XSetForeground(display, gc, 0xFFFF00);
            XFillArc(display, window, gc, cx - innerRadius, cy - innerRadius, innerRadius*2, innerRadius*2, 0, 360*64);
        }
        if (dragPiece == KING1 || dragPiece == KING2) {
            int innerRadius = radius / 2;
            if (dragPiece == KING1)
                XSetForeground(display, gc, 0xFF0000);
            else if (dragPiece == KING2)
                XSetForeground(display, gc, 0x0000FF);
            XFillArc(display, window, gc, cx - radius, cy - radius, radius*2, radius*2, 0, 360*64);
        }
    }
    // Indicatorul de mutare
    string turnText = "Randul jucatorului: ";
    turnText += (currentPlayer == PLAYER1) ? "PLAYER 1" : "PLAYER 2";
    XSetForeground(display, gc, 0x000000);
    XDrawString(display, window, gc, BOARD_OFFSET_X, BOARD_OFFSET_Y - 20, turnText.c_str(), turnText.length());
    
    // Buton pentru salvarea mutarilor
    int btnX = BOARD_OFFSET_X;
    int btnY = BOARD_OFFSET_Y + BOARD_SIZE * SQUARE_SIZE + 10;
    int btnWidth = BOARD_SIZE * SQUARE_SIZE;
    int btnHeight = 30;
    XSetForeground(display, gc, 0xCCCCCC);
    XFillRectangle(display, window, gc, btnX, btnY, btnWidth, btnHeight);
    XSetForeground(display, gc, 0x000000);
    string btnText = "Salveaza mutarile";
    int textX = btnX + 10;
    int textY = btnY + 20;
    XDrawString(display, window, gc, textX, textY, btnText.c_str(), btnText.length());
    
    XFlush(display);
}

bool onSaveButton(int x, int y) {
    int btnX = BOARD_OFFSET_X;
    int btnY = BOARD_OFFSET_Y + BOARD_SIZE * SQUARE_SIZE + 10;
    int btnWidth = BOARD_SIZE * SQUARE_SIZE;
    int btnHeight = 30;
    return (x >= btnX && x <= btnX + btnWidth && y >= btnY && y <= btnY + btnHeight);
}

// Validare mutare: verifica daca o mutare de la (fromRow,fromCol) la (toRow,toCol)
// este valida pentru piesa player. Daca este o capturare, se setează isCapture = true
// si se returneaza coordonatele piesei capturate.
bool validMove(int fromRow, int fromCol, int toRow, int toCol, int player, bool &isCapture, int &capRow, int &capCol) {
    isCapture = false;
    // Verificam daca destinatia este valida
    if (toRow < 0 || toRow >= BOARD_SIZE || toCol < 0 || toCol >= BOARD_SIZE)
        return false;
    if (board[toRow][toCol] != EMPTY)
        return false;
    // Se permite mutarea doar pe patratele "colorate"
    if ((toRow + toCol) % 2 == 0)
        return false;
    
    int dr = toRow - fromRow;
    int dc = toCol - fromCol;
    
    // Pentru "men" 
    if (player == PLAYER1 || player == PLAYER2) {
        if (player == PLAYER1) {
            // Mutare simpla: 1 patrat in diagonala înainte
            if (dr == -1 && abs(dc) == 1)
                return true;
            // Capturare: sare doua patrate in diagonala, piesa intermediara trebuie sa fie a inamicului
            if (dr == -2 && abs(dc) == 2) {
                int midRow = fromRow - 1;
                int midCol = fromCol + dc/2;
                if (board[midRow][midCol] == PLAYER2 || board[midRow][midCol] == KING2) {
                    isCapture = true;
                    capRow = midRow;
                    capCol = midCol;
                    return true;
                }
            }
        } else if (player == PLAYER2) {
            if (dr == 1 && abs(dc) == 1)
                return true;
            if (dr == 2 && abs(dc) == 2) {
                int midRow = fromRow + 1;
                int midCol = fromCol + dc/2;
                if (board[midRow][midCol] == PLAYER1 || board[midRow][midCol] == KING1) {
                    isCapture = true;
                    capRow = midRow;
                    capCol = midCol;
                    return true;
                }
            }
        }
    }
    // Pentru dame
    if (player == KING1 || player == KING2) {
        // Permitem mutari simple in toate directiile cu un pas
        if (abs(dr) == 1 && abs(dc) == 1)
            return true;
        // Capturare cu un salt de doua patrate in orice directie
        if (abs(dr) == 2 && abs(dc) == 2) {
            int midRow = fromRow + dr/2;
            int midCol = fromCol + dc/2;
            if (player == KING1 && (board[midRow][midCol] == PLAYER2 || board[midRow][midCol] == KING2)) {
                isCapture = true;
                capRow = midRow;
                capCol = midCol;
                return true;
            }
            if (player == KING2 && (board[midRow][midCol] == PLAYER1 || board[midRow][midCol] == KING1)) {
                isCapture = true;
                capRow = midRow;
                capCol = midCol;
                return true;
            }
        }
    }
    return false;
}

bool hasCaptureMove(int row, int col, int player) {
    bool dummy;
    int capRow, capCol;

    if (player == KING1 || player == KING2) {
        // Pentru dame, verificam toate cele patru directii cu un salt de două patrate.
        int drs[4] = {-2, -2, 2, 2};
        int dcs[4] = {-2, 2, -2, 2};
        for (int k = 0; k < 4; k++) {
            int newRow = row + drs[k];
            int newCol = col + dcs[k];
            if (validMove(row, col, newRow, newCol, player, dummy, capRow, capCol) && dummy)
                return true;
        }
    } else {
        // Pentru piesele normale (men), verificam doar direcția „inainte”
        int dr = (player == PLAYER1) ? -2 : 2;
        int dcs[2] = {-2, 2};
        for (int i = 0; i < 2; i++) {
            int newRow = row + dr;
            int newCol = col + dcs[i];
            if (validMove(row, col, newRow, newCol, player, dummy, capRow, capCol) && dummy)
                return true;
        }
    }
    return false;
}


// Verifica daca jucatorul poate efectua cel putin o mutare
bool hasPieces(int player) {
    if(dragging)
    {
        return true;
    }
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (player == PLAYER1) {
                if (board[i][j] == PLAYER1 || board[i][j] == KING1)
                    return true;
            } else if (player == PLAYER2) {
                if (board[i][j] == PLAYER2 || board[i][j] == KING2)
                    return true;
            }
        }
    }
    return false;
}


void switchTurn() {
    currentPlayer = (currentPlayer == PLAYER1) ? PLAYER2 : PLAYER1;
    multiCaptureActive = false;
    activePieceRow = -1;
    activePieceCol = -1;
}

void saveMovesToFile() {
    cout << "Introduceti numele fisierului pentru salvare: ";
    string filename;
    cin >> filename;
    ofstream file(filename.c_str());
    if (!file) {
        cout << "Eroare la deschiderea fisierului pentru scriere.\n";
        return;
    }
    for (size_t i = 0; i < movesHistory.size(); i++) {
        file << "Mutare " << i+1 << ": (" << movesHistory[i].fromRow << ","
             << movesHistory[i].fromCol << ") -> ("
             << movesHistory[i].toRow << "," << movesHistory[i].toCol << ")";
        if (movesHistory[i].capture)
            file << " (capturare)";
        file << "\n";
    }
    file.close();
    cout << "Mutarile au fost salvate in fisierul " << filename << "\n";
}

void computerMove() {
    // Generam lista tuturor mutarilor valide pentru piesele calculatorului (PLAYER2 sau KING2)
    vector<Move> validMoves;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if ((currentPlayer == PLAYER2) && (board[i][j] == PLAYER2 || board[i][j] == KING2)) {
                // Pentru piesele normale
                if (board[i][j] == PLAYER2) {
                    int drSimple = 1;
                    int drCapture = 2;
                    int dcs[2] = {-1, 1};
                    // Mutari simple
                    for (int k = 0; k < 2; k++) {
                        int newRow = i + drSimple;
                        int newCol = j + dcs[k];
                        bool cap;
                        int dum1, dum2;
                        if (validMove(i, j, newRow, newCol, currentPlayer, cap, dum1, dum2)) {
                            Move m;
                            m.fromRow = i;
                            m.fromCol = j;
                            m.toRow = newRow;
                            m.toCol = newCol;
                            m.capture = cap;
                            validMoves.push_back(m);
                        }
                    }
                    // Mutari de capturare pentru piese normale
                    for (int k = 0; k < 2; k++) {
                        int newRow = i + drCapture;
                        int newCol = j + 2*dcs[k];
                        bool cap;
                        int capRow, capCol;
                        if (validMove(i, j, newRow, newCol, currentPlayer, cap, capRow, capCol) && cap) {
                            Move m;
                            m.fromRow = i;
                            m.fromCol = j;
                            m.toRow = newRow;
                            m.toCol = newCol;
                            m.capture = true;
                            validMoves.push_back(m);
                        }
                    }
                }
                // Pentru dame
                if (board[i][j] == KING2) {
                    int drsSimple[4] = {-1, -1, 1, 1};
                    int dcsSimple[4] = {-1, 1, -1, 1};
                    for (int k = 0; k < 4; k++) {
                        int newRow = i + drsSimple[k];
                        int newCol = j + dcsSimple[k];
                        bool cap;
                        int dum1, dum2;
                        if (validMove(i, j, newRow, newCol, KING2, cap, dum1, dum2)) {
                            Move m;
                            m.fromRow = i;
                            m.fromCol = j;
                            m.toRow = newRow;
                            m.toCol = newCol;
                            m.capture = cap;
                            validMoves.push_back(m);
                        }
                    }
                    // Mutari de capturare pentru dame in toate cele patru directii
                    int drsCapture[4] = {-2, -2, 2, 2};
                    int dcsCapture[4] = {-2, 2, -2, 2};
                    for (int k = 0; k < 4; k++) {
                        int newRow = i + drsCapture[k];
                        int newCol = j + dcsCapture[k];
                        bool cap;
                        int capRow, capCol;
                        if (validMove(i, j, newRow, newCol, KING2, cap, capRow, capCol) && cap) {
                            Move m;
                            m.fromRow = i;
                            m.fromCol = j;
                            m.toRow = newRow;
                            m.toCol = newCol;
                            m.capture = true;
                            validMoves.push_back(m);
                        }
                    }
                }
            }
        }
    }
    
    // Daca exista cel putin o mutare de capturare, filtram lista pentru a pastra numai capturarile
    vector<Move> captureMoves;
    for (size_t i = 0; i < validMoves.size(); i++) {
        if (validMoves[i].capture)
            captureMoves.push_back(validMoves[i]);
    }
    if (!captureMoves.empty()) {
        validMoves = captureMoves;
    }
    
    if (validMoves.empty()) return;
    
    srand(time(NULL));
    int idx = rand() % validMoves.size();
    Move chosen = validMoves[idx];
    bool dummy;
    int capRow, capCol;
    int pieceType = board[chosen.fromRow][chosen.fromCol];
    if (chosen.capture) {
        validMove(chosen.fromRow, chosen.fromCol, chosen.toRow, chosen.toCol, pieceType, dummy, capRow, capCol);
        board[capRow][capCol] = EMPTY;
    }
    board[chosen.toRow][chosen.toCol] = board[chosen.fromRow][chosen.fromCol];
    board[chosen.fromRow][chosen.fromCol] = EMPTY;
    movesHistory.push_back(chosen);
    
    // Promovare: daca piesa a ajuns pe ultimul rand pentru calculator (PLAYER2)
    if (currentPlayer == PLAYER2 && chosen.toRow == BOARD_SIZE - 1 
        && board[chosen.toRow][chosen.toCol] == PLAYER2) {
        board[chosen.toRow][chosen.toCol] = KING2;
    }
    
    if (chosen.capture && hasCaptureMove(chosen.toRow, chosen.toCol, board[chosen.toRow][chosen.toCol])) {

    } else {
        switchTurn();
    }
}

int main() {
    cout << "Alegeti modul de joc: 1 - doi jucatori, 2 - impotriva calculatorului: ";
    int mod;
    cin >> mod;
    vsComputer = (mod == 2);
    
    initBoard();
    
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        cerr << "Nu se poate deschide display-ul X.\n";
        exit(1);
    }
    screen = DefaultScreen(display);
    window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, WINDOW_WIDTH, WINDOW_HEIGHT, 1, 
                                 BlackPixel(display, screen), 0x654321);
    XSelectInput(display, window, ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(display, window);
    
    gc = XCreateGC(display, window, 0, 0);
    
    XEvent event;
    while (1) {
        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == Expose) {
                drawBoard();
            } else if (event.type == ButtonPress) {
                int x = event.xbutton.x;
                int y = event.xbutton.y;
                // Daca s-a apasat pe butonul de salvare
                if (onSaveButton(x, y)) {
                    saveMovesToFile();
                    continue;
                }
                // Verificam daca s-a dat click pe o piesa
                int col = (x - BOARD_OFFSET_X) / SQUARE_SIZE;
                int row = (y - BOARD_OFFSET_Y) / SQUARE_SIZE;
                if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE) {
                    if (board[row][col] == currentPlayer || board[row][col] == (currentPlayer == PLAYER1 ? KING1 : KING2)) {
                        if (multiCaptureActive && (row != activePieceRow || col != activePieceCol))
                        {
                            continue;
                        }
                        dragging = true;
                        dragFromRow = row;
                        dragFromCol = col;
                        dragPiece = board[row][col];
                        board[row][col] = EMPTY;
                        dragOffsetX = x - (BOARD_OFFSET_X + col * SQUARE_SIZE + SQUARE_SIZE/2);
                        dragOffsetY = y - (BOARD_OFFSET_Y + row * SQUARE_SIZE + SQUARE_SIZE/2);
                    }
                }
            } else if (event.type == MotionNotify) {
                if (dragging) {
                    currentDragX = event.xmotion.x - dragOffsetX;
                    currentDragY = event.xmotion.y - dragOffsetY;
                    drawBoard();
                }
            } else if (event.type == ButtonRelease) {
                if (dragging) {
                    int x = event.xbutton.x;
                    int y = event.xbutton.y;
                    int col = (x - BOARD_OFFSET_X) / SQUARE_SIZE;
                    int row = (y - BOARD_OFFSET_Y) / SQUARE_SIZE;
                    bool valid = false;
                    bool isCap = false;
                    int capRow, capCol;
                    if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE) {
                        int pieceType;
                        pieceType = (dragPiece == KING1 || dragPiece == KING2) ? dragPiece : currentPlayer;
                        if (multiCaptureActive) {
                            if (validMove(activePieceRow, activePieceCol, row, col, pieceType, isCap, capRow, capCol) && isCap)
                                valid = true;
                        } else {
                            if (validMove(dragFromRow, dragFromCol, row, col, pieceType, isCap, capRow, capCol))
                                valid = true;
                        }
                    }
                    
                    if (valid) {
                        // Promovare: daca piesa ajunge pe ultimul rand opus, se promoveaza la dama
                        if (currentPlayer == PLAYER1 && row == 0 && (dragPiece == PLAYER1)) {
                            dragPiece = KING1;
                        } else if (currentPlayer == PLAYER2 && row == BOARD_SIZE - 1 && (dragPiece == PLAYER2)) {
                            dragPiece = KING2;
                        }
                        Move m;
                        m.fromRow = (multiCaptureActive ? activePieceRow : dragFromRow);
                        m.fromCol = (multiCaptureActive ? activePieceCol : dragFromCol);
                        m.toRow = row;
                        m.toCol = col;
                        m.capture = isCap;
                        movesHistory.push_back(m);
                        if (isCap) {
                            board[capRow][capCol] = EMPTY;
                        }
                        board[row][col] = dragPiece;
                        // Daca a fost capturare si se poate continua, intram in modul de capturare in cascada
                        if (isCap && hasCaptureMove(row, col, board[row][col]))
                        {
                            multiCaptureActive = true;
                            activePieceRow = row;
                            activePieceCol = col;
                        } else {
                            multiCaptureActive = false;
                            activePieceRow = -1;
                            activePieceCol = -1;
                            switchTurn();
                        }
                    } else {
                        // Mutare invalida: returnam piesa la locul initial
                        if (multiCaptureActive)
                            board[activePieceRow][activePieceCol] = dragPiece;
                        else
                            board[dragFromRow][dragFromCol] = dragPiece;
                    }
                    dragging = false;
                    dragPiece = EMPTY;
                    drawBoard();
                }
            }
        }
        // Daca modul de joc este versus calculator si este randul calculatorului
        if (vsComputer && currentPlayer == PLAYER2 && !dragging) {
            usleep(300000); // mic delay
            computerMove();
            drawBoard();
        }

        // Verificare conditii de victorie: daca unul dintre jucttori nu mai are mutari posibile
        if (!hasPieces(PLAYER1)) {
            XSetForeground(display, gc, 0x00FF00);
            string winText = "PLAYER 2 castiga!";
            XDrawString(display, window, gc, BOARD_OFFSET_X, 30, winText.c_str(), winText.length());
            XFlush(display);
            break;
        }
        if (!hasPieces(PLAYER2)) {
            XSetForeground(display, gc, 0x00FF00);
            string winText = "PLAYER 1 castiga!";
            XDrawString(display, window, gc, BOARD_OFFSET_X, 30, winText.c_str(), winText.length());
            XFlush(display);
            break;
        }
    }
    sleep(5);
    XCloseDisplay(display);
    return 0;
}
