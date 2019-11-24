#pragma once
#include <vector>

enum ChessPice {
    Non = 0,
    BlackPawn = 1,
    BlackKnight = 2,
    BlackBishop = 3,
    BlackRook = 4,
    BlackQueen = 5,
    BlackKing = 6,
    WhitePawn = 7,
    WhiteKnight = 8,
    WhiteBishop = 9,
    WhiteRook = 10,
    WhiteQueen = 11,
    WhiteKing = 12
};

struct Field {
    public:
    const std::string* fieldname; //pointer to string in BoardPos
    bool AnPassan_able;
    bool FirstMove;
    bool PiceCapturedLastMove;
    ChessPice Pice;

    Field(ChessPice pice, bool anPassanAble, bool firstMove, bool piceCapturedLastMove)
    {
        Pice = pice;
        AnPassan_able = anPassanAble;
        FirstMove = firstMove; //first move means an passant creating if pawn and castleable if rook or king
        PiceCapturedLastMove = piceCapturedLastMove;
    }

    Field(ChessPice pice) : Field(pice, false, false, false)
    {

    }

    Field() : Field(ChessPice::Non)
    {

    }

    bool HasBlackPice()
    {
       switch (Pice)
        {
            case BlackKing:
            case BlackQueen:
            case BlackBishop:
            case BlackKnight:
            case BlackRook:
            case BlackPawn:
                return true;
            default:
                return false;
        }
    }

    bool HasWhitePice()
    {
        switch (Pice)
        {
            case WhiteKing:
            case WhiteQueen:
            case WhiteBishop:
            case WhiteKnight:
            case WhiteRook:
            case WhitePawn:
                return true;
            default:
                return false;
        }
    }

    static bool BlackPice(ChessPice pice)
    {
        switch (pice)
        {
            case BlackKing:
            case BlackQueen:
            case BlackBishop:
            case BlackKnight:
            case BlackRook:
            case BlackPawn:
                return true;
            default:
                return false;
        }
    }

    static bool WhitePice(ChessPice pice)
    {
        switch (pice)
        {
            case WhiteKing:
            case WhiteQueen:
            case WhiteBishop:
            case WhiteKnight:
            case WhiteRook:
            case WhitePawn:
                return true;
            default:
                return false;
        }
    }

    static char PiceChar(ChessPice pice)
    {
        switch (pice)
        {
            case WhiteKing:
                return 'K';
            case WhiteQueen:
                return 'Q';
            case WhiteBishop:
                return 'B';
            case WhiteKnight:
                return 'N';
            case WhiteRook:
                return 'R';
            case WhitePawn:
                return 'P';
            case BlackKing:
                return 'k';
            case BlackQueen:
                return 'q';
            case BlackBishop:
                return 'b';
            case BlackKnight:
                return 'n';
            case BlackRook:
                return 'r';
            case BlackPawn:
                return 'p';
            case Non:
                return ' ';
            default:
                return 'E';
        }
    }
};

