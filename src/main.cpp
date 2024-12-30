#include <bit>
#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <random>
#include <sys/types.h>


using Bitboard = uint16_t;
#define bitloop(X) for (; (X); (X) = _blsr_u64(X))

enum class GameStatus {
    IN_PROGRESS,
    DRAW,
    CIRCLE_WON,
    CROSS_WON,
};

struct Board {
    Bitboard cross;
    Bitboard circle;

    bool cross_to_move;

    [[nodiscard]] Bitboard get_legal_moves() const { return ~(cross | circle) & 0b111111111; }
    void make_move(Bitboard move) {
        if (cross_to_move) {
            cross ^= move;
        } else {
            circle ^= move;
        }
        cross_to_move = !cross_to_move;
    }

    void unmake_move(Bitboard move) {
        if (!cross_to_move) {
            cross ^= move;
        } else {
            circle ^= move;
        }
        cross_to_move = !cross_to_move;
    }

    [[nodiscard]] GameStatus check_status() const {
        constexpr Bitboard winning_patterns[] = {
                0b111000000, // top row
                0b000111000, // middle row
                0b000000111, // bottom row
                0b100100100, // left column
                0b010010010, // middle column
                0b001001001, // right column
                0b100010001, // diagonal
                0b001010100, // antidiagonal
        };

        for (const auto pattern: winning_patterns) {
            if ((cross & pattern) == pattern) return GameStatus::CROSS_WON;
            if ((circle & pattern) == pattern) return GameStatus::CIRCLE_WON;
        }

        // check unwinnable
        bool winnable = false;
        for (const auto pattern: winning_patterns) {
            if (((cross | ~circle) & pattern) == pattern) winnable = true;
            if (((circle | ~cross) & pattern) == pattern) winnable = true;
        }

        if (winnable) return GameStatus::IN_PROGRESS;
        return GameStatus::DRAW;
    }

    void show() const {
        //
        //    │   │
        // ───┼───┼───
        //    │   │
        // ───┼───┼───
        //    │   │
        for (int rank = 2; rank >= 0; rank--) {
            for (int file = 0; file < 3; file++) {
                int sq = rank * 3 + file;
                Bitboard sq_bb = 1 << sq;
                if (sq_bb & cross) {
                    std::cout << " ✗ ";
                } else if (sq_bb & circle) {
                    std::cout << " ◯ ";
                } else {
                    std::cout << " \033[38;2;86;95;137m" << sq << "\033[0m ";
                }
                if (file != 2) {
                    std::cout << "│";
                }
            }
            std::cout << "\n";
            if (rank != 0) {
                std::cout << "───┼───┼───\n";
            }
        }
    }
};

class Player {
public:
    virtual ~Player() = default;
    virtual Bitboard choose_move(const Board& board) = 0;
    virtual std::string get_name() = 0;
};

class HumanPlayer : public Player {
private:
    std::string m_name;

public:
    explicit HumanPlayer(std::string name) : m_name(std::move(name)) {}


    Bitboard choose_move(const Board& board) override {
        int number;

        const Bitboard legal_moves = board.get_legal_moves();

        while (true) {
            std::cout << "Cell (0 - 8): ";
            std::cin >> number;

            if (std::cin.fail() || number < 0 || number > 8 || ((1ull << number) & legal_moves) == 0) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid input. Please try again.\n";
            } else {
                break;
            }
        }

        return 1 << number;
    }

    std::string get_name() override { return m_name; }
};


class RandomPlayer : public Player {
private:
    std::string m_name;

public:
    explicit RandomPlayer(std::string name) : m_name(std::move(name)) {}

    Bitboard choose_move(const Board& board) override {
        std::random_device rd;
        std::default_random_engine eng(rd());
        std::uniform_int_distribution<int> distr(0, 8);

        const Bitboard legal_moves = board.get_legal_moves();

        int number = distr(eng);

        while (number != -1 && ((1ull << number) & legal_moves) == 0) {
            number = distr(eng);
        }

        return 1 << number;
    }

    std::string get_name() override { return m_name; }
};

int alphabeta(Board& board, int alpha, int beta, int depth) {
    const GameStatus result = board.check_status();
    switch (result) {
        case GameStatus::DRAW:
            return 0;
        case GameStatus::CIRCLE_WON:
            if (board.cross_to_move)
                return -1;
            else
                return 10 - depth;
        case GameStatus::CROSS_WON:
            if (board.cross_to_move)
                return 10 - depth;
            else
                return -1;
        default:
            break;
    }

    Bitboard legal_moves = board.get_legal_moves();
    bitloop(legal_moves) {
        const Bitboard move_bb = _blsi_u64(legal_moves);
        board.make_move(move_bb);
        const int score = -alphabeta(board, -beta, -alpha, depth + 1);
        board.unmake_move(move_bb);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

class BotPlayer : public Player {
private:
    std::string m_name;

public:
    explicit BotPlayer(std::string name) : m_name(std::move(name)) {}

    Bitboard choose_move(const Board& board) override {
        Board tmp_board = board;
        Bitboard legal_moves = board.get_legal_moves();
        Bitboard best_move = 0;
        int best_score = -100;

        bitloop(legal_moves) {
            const Bitboard move_bb = _blsi_u64(legal_moves);
            tmp_board.make_move(move_bb);
            const int score = -alphabeta(tmp_board, -100, 100, 0);
            tmp_board.unmake_move(move_bb);

            if (score > best_score) {
                best_score = score;
                best_move = move_bb;
            } else if (score == best_score) {
                best_move |= move_bb;
            }
        }

        if (best_score > 0) {
            std::cout << "Bot gewinnt auf jeden fall\n";
        } else if (best_score == 0) {
            std::cout << "Unentschieden bis jetzt\n";
        } else {
            std::cout << "Bot verliert bei perfektem Spiel\n";
        }


        int move_count = std::popcount(best_move);

        if (move_count <= 1) return best_move;

        std::random_device rd;
        std::default_random_engine eng(rd());
        std::uniform_int_distribution<int> distr(1, move_count - 1);

        int des_bit = distr(eng);

        Bitboard mask = 0b1;
        int bit_set = 0;
        for (int i = 0; i < 9; i++) {
            if (mask & best_move) {
                bit_set++;
            }

            if (des_bit == bit_set) {
                return mask;
            }

            mask <<= 1;
        }

        return 0;
    }

    std::string get_name() override { return m_name; }
};
GameStatus play(Player& cross, Player& circle, Board& board) {
    int count = 0;
    while (board.check_status() == GameStatus::IN_PROGRESS) {
        std::cout << "\n";
        board.show();
        std::cout << "\n";
        if (count++ % 2 == 0) {
            std::cout << cross.get_name() << " (✗)\n";
            const Bitboard p1_move = cross.choose_move(board);
            board.make_move(p1_move);
            continue;
        }

        std::cout << circle.get_name() << "(◯)\n\n";
        const Bitboard p2_move = circle.choose_move(board);
        board.make_move(p2_move);
    }

    board.show();

    return board.check_status();
}

int main() {
    auto board = Board{.cross = 0, .circle = 0, .cross_to_move = true};

    auto cr = HumanPlayer("Kolia");
    auto ci = BotPlayer("Silas");

    std::cout << "✗: " << cr.get_name() << "\n";
    std::cout << "◯: " << ci.get_name() << "\n\n";

    const GameStatus result = play(cr, ci, board);

    std::cout << "\n";
    switch (result) {
        case GameStatus::DRAW:
            std::cout << "======== DRAW ========\n";
            break;
        case GameStatus::CIRCLE_WON:
            std::cout << "======== " << ci.get_name() << " WON ========\n";
            break;
        case GameStatus::CROSS_WON:
            std::cout << "======== " << cr.get_name() << " WON ========\n";
            break;
        default:
            break;
    }
    std::cout << "\n";
}
