#include <chrono>
#include <thread>

// Класс для представления игрового поля с простым ИИ 
//!самое важное тут оценка позиции, алгоритм оценки и вес ходов, если грамотно настроить и убрать ненужное загромождение это усилит бота значительно.
class RenjuBoard {
    private:
        static const int SIZE = 31;
        char board_data[SIZE][SIZE]; // ' ' - пусто, 'B' - черные, 'W' - белые
        const int dx[8] = {1, 0, 1, 1, -1, 0, -1, -1}; //направления во все 8 стороны
        const int dy[8] = {0, 1, 1, -1, 0, -1, -1, 1};
    public:
    bool is_first_move;
        RenjuBoard() { reset(); }
        void reset() {
            for (int i = 0; i < SIZE; ++i)
                for (int j = 0; j < SIZE; ++j)
                    board_data[i][j] = ' ';
            is_first_move = true;
        }
        bool is_valid_move(int x, int y) {// иногда где-то тут что-то косит
            return x >= 0 && x < SIZE && y >= 0 && y < SIZE && board_data[x][y] == ' ';
        }
        void make_move(int x, int y, char color) {
            if (is_valid_move(x, y)) {
                board_data[x][y] = color;
                is_first_move = false;
            }
        }
        std::pair<int, int> get_first_move() const { 
            // std::cerr << "перв ход: center" << std::endl;
            return {15, 15}; 
        }
        // Генерация кандидатных ходов (в радиусе 2 от занятых) по ощущениям нужно 3
        std::vector<std::pair<int, int>> get_candidate_moves() const {
            static const int RADIUS = 2;
            std::vector<std::pair<int, int>> candidates;
            bool used[SIZE][SIZE] = {};
            
            // Сначала ходы в центре (приоритет для белых)
            for (int i = 11; i <= 19; ++i) {
                for (int j = 11; j <= 19; ++j) {
                    if (board_data[i][j] == ' ' && !used[i][j]) {
                        used[i][j] = true;
                        candidates.emplace_back(i, j);
                    }
                }
            }
            
            // добавляем ходы вокруг камней
            for (int i = 0; i < SIZE; ++i) {
                for (int j = 0; j < SIZE; ++j) {
                    if (board_data[i][j] != ' ') {
                        for (int dx = -RADIUS; dx <= RADIUS; ++dx) {
                            for (int dy = -RADIUS; dy <= RADIUS; ++dy) {
                                int ni = i + dx, nj = j + dy;
                                if (ni >= 0 && ni < SIZE && nj >= 0 && nj < SIZE && board_data[ni][nj] == ' ' && !used[ni][nj]) {
                                    used[ni][nj] = true;
                                    candidates.emplace_back(ni, nj);
                                }
                            }
                        }
                    }
                }
            }
            
            // доска пуста — вернуть центр
            if (candidates.empty()) candidates.emplace_back(15, 15);// иногда глючит после резкого рестарта wsl to cmd -> ubuntu --shutdown
            return candidates;
        }
            
        // оценки для черныхи белых 
        long long evaluate_advanced_position(char player, bool is_black) const {
            char opponent = (player == 'B') ? 'W' : 'B';
            long long score = 0;
            // Центр поля — бонус, край — штраф
            for (int x = 0; x < SIZE; ++x) {
                for (int y = 0; y < SIZE; ++y) {
                    if (board_data[x][y] == player) {
                        int center_dist = abs(x - SIZE/2) + abs(y - SIZE/2);
                        score += (SIZE - center_dist) * 2;
            }
                    if (board_data[x][y] == opponent) {
                        int center_dist = abs(x - SIZE/2) + abs(y - SIZE/2);
                        score -= (SIZE - center_dist) * 2;
            }
                }
            }
            // белые: защита + контратака +центр
            if (!is_black) {
                // Бонус за центр
                int center_bonus = 0;
                for (int x = 12; x <= 18; ++x) {
                    for (int y = 12; y <= 18; ++y) {
                        if (board_data[x][y] == player) {
                            center_bonus += 5000;
                        } else if (board_data[x][y] == opponent) {
                            center_bonus -= 3000;
                        }
                    }
                }
                score += center_bonus;
                
                // Проверка критических угроз опонента
                for (int x = 0; x < SIZE; ++x) {
                    for (int y = 0; y < SIZE; ++y) {
                        if (board_data[x][y] == opponent) {
                            for (int dir = 0; dir < 4; ++dir) {
                                // Проверка всех 5 клеток в стороны
                                for (int shift = -4; shift <= 0; ++shift) {
                                    int stones = 0, empties = 0, blocked = 0;
                                    for (int k = 0; k < 5; ++k) {
                                        int nx = x + dx[dir] * (shift + k);
                                        int ny = y + dy[dir] * (shift + k);
                                        if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                                        if (board_data[nx][ny] == opponent) stones++;
                                        else if (board_data[nx][ny] == ' ') empties++;
                                        else { blocked++; break; }
                                    }
                                    // Критические угрозы
                                    if (stones == 4 && empties == 1) return -9900000; // открытая четверка
                                    if (stones == 5) return -10000000; // пятерка
                                    if (stones == 3 && empties == 2 && blocked == 0) return -8000000; // открытая тройка
                                }
                                
                                // Проверка двойных угроз
                                for (int shift1 = -4; shift1 <= 0; ++shift1) {
                                    for (int shift2 = shift1 + 1; shift2 <= 0; ++shift2) {
                                        int stones1 = 0, empties1 = 0, blocked1 = 0;
                                        int stones2 = 0, empties2 = 0, blocked2 = 0;
                                        
                                        // перв угроза
                                        for (int k = 0; k < 5; ++k) {
                                            int nx = x + dx[dir] * (shift1 + k);
                                            int ny = y + dy[dir] * (shift1 + k);
                                            if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked1++; break; }
                                            if (board_data[nx][ny] == opponent) stones1++;
                                            else if (board_data[nx][ny] == ' ') empties1++;
                                            else { blocked1++; break; }
                                        }
                                        
                                        // Вторая угроза
                                        for (int k = 0; k < 5; ++k) {
                                            int nx = x + dx[dir] * (shift2 + k);
                                            int ny = y + dy[dir] * (shift2 + k);
                                            if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked2++; break; }
                                            if (board_data[nx][ny] == opponent) stones2++;
                                            else if (board_data[nx][ny] == ' ') empties2++;
                                            else { blocked2++; break; }
                                        }
                                        
                                        // 2-ная угроза
                                        if ((stones1 == 3 && empties1 == 2 && blocked1 == 0) && 
                                            (stones2 == 3 && empties2 == 2 && blocked2 == 0)) {
                                            return -9000000;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Проверка форсированных последовательностей
                for (int x = 0; x < SIZE; ++x) {
                    for (int y = 0; y < SIZE; ++y) {
                        if (board_data[x][y] == opponent) {
                            // Проверка в 8 направлениях
                            for (int dir = 0; dir < 8; ++dir) {
                                int count = 1, open_ends = 0, blocked = 0;
                                
                                // В одну сторону
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[dir] * i;
                                    int ny = y + dy[dir] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                                    if (board_data[nx][ny] == opponent) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else { blocked++; break; }
                                }
                                
                                // В противоположную сторону
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[(dir + 4) % 8] * i;
                                    int ny = y + dy[(dir + 4) % 8] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                                    if (board_data[nx][ny] == opponent) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else { blocked++; break; }
                                }
                                
                                // Критические паттерны
                                if (count >= 5) return -10000000; // пятерка
                                if (count == 4 && open_ends >= 1) return -9500000; // четверка с пробелом
                                if (count == 3 && open_ends == 2 && blocked == 0) return -8500000; // открытая тройка
                                if (count == 3 && open_ends == 1) return -5000000; // закрытая тройка
                            }
                        }
                    }
                }
                
                // Агрес контратака для б
                int attack_opportunities = 0;
                int defensive_moves = 0;
                
                // Ищем возможн атаку б
                for (int x = 0; x < SIZE; ++x) {
                    for (int y = 0; y < SIZE; ++y) {
                        if (board_data[x][y] == player) {
                            for (int dir = 0; dir < 4; ++dir) {
                                int count = 1, open_ends = 0;
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[dir] * i;
                                    int ny = y + dy[dir] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                    if (board_data[nx][ny] == player) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else break;
                                }
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[dir + 4] * i;
                                    int ny = y + dy[dir + 4] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                    if (board_data[nx][ny] == player) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else break;
                                }
                                
                                // оценка атак
                                if (count >= 4 && open_ends >= 1) attack_opportunities += 50000; // четверка
                                else if (count >= 3 && open_ends >= 2) attack_opportunities += 15000; // открытая тройка
                                else if (count >= 3 && open_ends >= 1) attack_opportunities += 5000; // закрытая тройка
                                else if (count >= 2 && open_ends >= 2) attack_opportunities += 1000; // открытая двойка
                            }
                        }
                    }
                }
                
                // Ищем блокирующие ходы
                for (int x = 0; x < SIZE; ++x) {
                    for (int y = 0; y < SIZE; ++y) {
                        if (board_data[x][y] == opponent) {
                            for (int dir = 0; dir < 4; ++dir) {
                                int count = 1, open_ends = 0;
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[dir] * i;
                                    int ny = y + dy[dir] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                    if (board_data[nx][ny] == opponent) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else break;
                                }
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[dir + 4] * i;
                                    int ny = y + dy[dir + 4] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                    if (board_data[nx][ny] == opponent) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else break;
                                }
                                
                                // Штраф за угрозы соперника
                                if (count >= 3 && open_ends >= 1) defensive_moves -= count * 2000;
                            }
                        }
                    }
                }
                
                score += attack_opportunities + defensive_moves;
            }
            
            // стратегия для черных: атака + контроль центра
            if (is_black) {            // Бонус за контроль центра для черных(чуть меньше)
                long long center_bonus = 0;
                for (int x = 12; x <= 18; ++x) {
                    for (int y = 12; y <= 18; ++y) {
                        if (board_data[x][y] == player) {
                            center_bonus += 800; // Больше чем у белых
                        } else if (board_data[x][y] == opponent) {
                            center_bonus -= 2000;
                        }
                    }
                }
                score += center_bonus;
                
                // Проверка критических угроз ч
                for (int x = 0; x < SIZE; ++x) {
                    for (int y = 0; y < SIZE; ++y) {
                        if (board_data[x][y] == player) {
                            for (int dir = 0; dir < 4; ++dir) {
                                // Проверка всех 5 сторон камней
                                for (int shift = -4; shift <= 0; ++shift) {
                                    int stones = 0, empties = 0, blocked = 0;
                                    for (int k = 0; k < 5; ++k) {
                                        int nx = x + dx[dir] * (shift + k);
                                        int ny = y + dy[dir] * (shift + k);
                                        if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                                        if (board_data[nx][ny] == player) stones++;
                                        else if (board_data[nx][ny] == ' ') empties++;
                                        else { blocked++; break; }
                                    }
                                    // Крит угрозы черных
                                    if (stones == 4 && empties == 1) return 9900000; 
                                    if (stones == 5) return 10000000; // пятерка
                                    if (stones == 3 && empties == 2 && blocked == 0) return 8000000; // открытая тройка
                                }
                                
                                // Проверка двойных угроз черных
                                for (int shift1 = -4; shift1 <= 0; ++shift1) {
                                    for (int shift2 = shift1 + 1; shift2 <= 0; ++shift2) {
                                        
                                        int stones1 = 0, empties1 = 0, blocked1 = 0; int stones2 = 0, empties2 = 0, blocked2 = 0;
                                        
                                        // 1 угроза
                                        for (int k = 0; k < 5; ++k) {
                                            int nx = x + dx[dir] * (shift1 + k);
                                            int ny = y + dy[dir] * (shift1 + k);
                                            if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked1++; break; }
                                            if (board_data[nx][ny] == player) stones1++;
                                            else if (board_data[nx][ny] == ' ') empties1++;
                                            else { blocked1++; break; }
                                        }
                                        
                                        // 2 угроза
                                        for (int k = 0; k < 5; ++k) {
                                            int nx = x + dx[dir] * (shift2 + k);
                                            int ny = y + dy[dir] * (shift2 + k);
                                            if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked2++; break; }
                                            if (board_data[nx][ny] == player) stones2++;
                                            else if (board_data[nx][ny] == ' ') empties2++;
                                            else { blocked2++; break; }
                                        }
                                        
                                        // 2-ная угроза черных
                                        if ((stones1 == 3 && empties1 == 2 && blocked1 == 0) && 
                                            (stones2 == 3 && empties2 == 2 && blocked2 == 0)) {
                                            return 9000000;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Пров форс последов черных
                for (int x = 0; x < SIZE; ++x) {
                    for (int y = 0; y < SIZE; ++y) {
                        if (board_data[x][y] == player) {
                            // Проверка в 8 направлен (право вверх почему-то не работает, мб где-то намудрено) в dx dy
                            for (int dir = 0; dir < 8; ++dir) {
                                int count = 1, open_ends = 0, blocked = 0;
                                
                                // В одну сторону
                                for (int i = 1; i <= 4; ++i) { //!!! это лишнее уже есть проверки на это, жрет время и память!!!
                                    int nx = x + dx[dir] * i;
                                    int ny = y + dy[dir] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                                    if (board_data[nx][ny] == player) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else { blocked++; break; }
                                }
                                
                                // в противоположную сторону
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[(dir + 4) % 8] * i;
                                    int ny = y + dy[(dir + 4) % 8] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                                    if (board_data[nx][ny] == player) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else { blocked++; break; }
                                }
                                
                                // Критические паттернн
                                if (count >= 5) return 10000000; // пятерка
                                if (count == 4 && open_ends >= 1) return 9500000; // четверка с пробелом
                                if (count == 3 && open_ends == 2 && blocked == 0) return 8500000; // открытая тройка
                                if (count == 3 && open_ends == 1) return 5000000; // закрытая тройка
                            }
                        }
                    }
                }
                
                // Агрессивная атака для черных
                long long attack_opportunities = 0;
                long long defensive_moves = 0;
                
                // Ищем атакующие возможности черных
                for (int x = 0; x < SIZE; ++x) {
                    for (int y = 0; y < SIZE; ++y) {
                        if (board_data[x][y] == player) {
                            for (int dir = 0; dir < 4; ++dir) {//!!! это лишнее уже есть проверки на это, жрет время и память!!!
                                int count = 1, open_ends = 0;
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[dir] * i;
                                    int ny = y + dy[dir] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                    if (board_data[nx][ny] == player) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else break;
                                }
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[dir + 4] * i;
                                    int ny = y + dy[dir + 4] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                    if (board_data[nx][ny] == player) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else break;
                                }
                                
                                // Усиленная оценка атак черных
                                if (count >= 4 && open_ends >= 1) attack_opportunities += 80000; // четверка
                                else if (count >= 3 && open_ends >= 2) attack_opportunities += 25000; // открытая тройка
                                else if (count >= 3 && open_ends >= 1) attack_opportunities += 8000; // закрытая тройка
                                else if (count >= 2 && open_ends >= 2) attack_opportunities += 2000; // открытая двойка
                            }
                        }
                    }
                }
                
                // Ищем блокирующие ходы против атак соперника
                for (int x = 0; x < SIZE; ++x) {
                    for (int y = 0; y < SIZE; ++y) {
                        if (board_data[x][y] == opponent) {
                            for (int dir = 0; dir < 4; ++dir) {
                                int count = 1, open_ends = 0;
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[dir] * i;
                                    int ny = y + dy[dir] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                    if (board_data[nx][ny] == opponent) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else break;
                                }
                                for (int i = 1; i <= 4; ++i) {
                                    int nx = x + dx[dir + 4] * i;
                                    int ny = y + dy[dir + 4] * i;
                                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                    if (board_data[nx][ny] == opponent) count++;
                                    else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                    else break;
                                }
                                
                                // Штраф за угрозы соперника
                                if (count >= 3 && open_ends >= 1) defensive_moves -= count * 3000;
                            }
                        }
                    }
                }
                
                score += attack_opportunities + defensive_moves;
            }
            
            // Паттерны: считаем для обеих сторон
            auto pattern_score = [&](char who, int sign) {
                int local_score = 0;
                int open_threes = 0, open_fours = 0, closed_fours = 0, fives = 0, sixes = 0, long_rows = 0;
                for (int x = 0; x < SIZE; ++x) {
                    for (int y = 0; y < SIZE; ++y) {
                        if (board_data[x][y] != who) continue;
                        for (int dir = 0; dir < 4; ++dir) {
                            int count = 1, open_ends = 0;
                            for (int i = 1; i < 6; ++i) {
                                int nx = x + dx[dir] * i, ny = y + dy[dir] * i;
                                if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                if (board_data[nx][ny] == who) count++;
                                else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                else break;
        }
                            for (int i = 1; i < 6; ++i) {
                                int nx = x + dx[dir + 4] * i, ny = y + dy[dir + 4] * i;
                                if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                if (board_data[nx][ny] == who) count++;
                                else if (board_data[nx][ny] == ' ') { open_ends++; break; }
                                else break;
            }
                            // Для черных — усилить атаку
                            if (is_black && sign == 1) {
                                if (count >= 6) { local_score += 15000000; sixes++; }//!!! это лишнее уже есть проверки на это, жрет время и память!!!
                                else if (count == 5) { local_score += 12000000; fives++; }
                                else if (count == 4 && open_ends == 2) { local_score += 4000000; open_fours++; }
                                else if (count == 4 && open_ends == 1) { local_score += 800000; closed_fours++; }
                                else if (count == 3 && open_ends == 2) { local_score += 800000; open_threes++; }
                                else if (count == 3 && open_ends == 1) { local_score += 30000; }
                                else if (count == 2 && open_ends == 2) { local_score += 8000; }
                                else if (count == 2 && open_ends == 1) { local_score += 1000; }
                                if (count > 6) { local_score += 15000000; long_rows++; }
                                // Бонус за центр
                                if (x >= 12 && x <= 18 && y >= 12 && y <= 18) local_score += 5000;
                                // Штраф за край
                                if (x <= 2 || x >= SIZE-3 || y <= 2 || y >= SIZE-3) local_score -= 2000;
                            } else {
                                // Агрессивная оценка для белых (защита + атака)
                                if (count >= 6) { local_score += sign * 10000000; sixes++; }
                                else if (count == 5) { local_score += sign * 9000000; fives++; }
                                else if (count == 4 && open_ends == 2) { local_score += sign * 1200000; open_fours++; }
                                else if (count == 4 && open_ends == 1) { local_score += sign * 80000; closed_fours++; }
                                else if (count == 3 && open_ends == 2) { local_score += sign * 50000; open_threes++; }
                                else if (count == 3 && open_ends == 1) { local_score += sign * 5000; }
                                else if (count == 2 && open_ends == 2) { local_score += sign * 2000; }
                                else if (count == 2 && open_ends == 1) { local_score += sign * 300; }
                                if (count > 6) { local_score += sign * 10000000; long_rows++; }
                            }
                            if (x == 0 || x == SIZE-1 || y == 0 || y == SIZE-1) local_score -= sign * 100;
                        }
                    }
                }
                // Форсированные угрозы
                if (is_black && sign == 1) {
                    if (open_fours >= 2) local_score += 5000000;
                    if (open_threes >= 2) local_score += 1000000;
                    if (closed_fours >= 2) local_score += 50000;
                    if (fives >= 2) local_score += 10000000;
                } else {
                    //  форсированные угрозы для белых
                    if (open_fours >= 2) local_score += sign * 2500000;
                    if (open_threes >= 2) local_score += sign * 120000;
                    if (closed_fours >= 2) local_score += sign * 15000;
                    if (fives >= 2) local_score += sign * 6000000;
                    if (sign == -1 && !is_black) {
                        // Доп штраф за угрозы соперника для белых
                        local_score -= open_threes * 150000;
                        local_score -= open_fours * 500000;
                    }
                }
                if (local_score > 12000000) local_score = 12000000;
                if (local_score < -12000000) local_score = -12000000;
                return local_score;
            };
            score += pattern_score(player, 1);
            score += pattern_score(opponent, -1);
            if (score > 15000000) score = 15000000;
            if (score < -15000000) score = -15000000;
            return score;
            }
            
        // Минимакс с альфа-бета отсечением и ограничением времени(СКОЛЬКО ТУТ БАГОВ, БОЖЕ БЛАГОСЛАВИ ЕГО НА СКОРОСТЬ И АНТИ-БАГАННОСТЬ)
        long long minimax(int depth, bool maximizing, char my_color, char opp_color, long long alpha, long long beta, std::chrono::steady_clock::time_point start_time, int time_limit_ms) {
            // Проверка времени
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            if (elapsed > time_limit_ms) {
                return evaluate_advanced_position(my_color, my_color == 'B'); //
            }
            
            // Проверка на победу
            for (int x = 0; x < SIZE; ++x)
                for (int y = 0; y < SIZE; ++y)
                    if (board_data[x][y] != ' ' && check_win(x, y, board_data[x][y]))
                        return (board_data[x][y] == my_color) ? 10000000 : -10000000;
            
            if (depth == 0) return evaluate_advanced_position(my_color, my_color == 'B');
            
            auto moves = get_candidate_moves();
            if (moves.empty()) return 0;
            
            // Сортировка кандидатов по локальной оценке
            std::vector<std::tuple<int, int, int>> scored_moves;
            char current = maximizing ? my_color : opp_color;
            for (auto [x, y] : moves) {
                int score = evaluate_simple_position(x, y, current);
                scored_moves.emplace_back(-score, x, y); // минус для сортировки по убыванию
            }
            std::sort(scored_moves.begin(), scored_moves.end());
            
            // Адаптивное ограничение кандидатов в minimax
            int max_candidates;
            if (depth <= 2) max_candidates = 12;
            else if (depth <= 4) max_candidates = 10;
            else if (depth <= 6) max_candidates = 8;
            else max_candidates = 6;
            
            if (scored_moves.size() > max_candidates) scored_moves.resize(max_candidates);
            
            if (maximizing) {
                long long maxEval = -100000000;
                for (auto [neg_score, x, y] : scored_moves) {
                    // Проверяем время реже для более глубокого анализа
                    static int time_check_counter = 0;
                    time_check_counter++;
                    if (time_check_counter % 15 == 0) { // Еще реже проверяем время
                        auto now = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                        if (elapsed > 4400) { // Используем 4 секунды из 4.5
                            break;
                        }
                    }
                    
                    if (!is_valid_move(x, y)) continue;
                    
                    make_move(x, y, my_color);
                    long long eval = minimax(depth-1, false, my_color, opp_color, alpha, beta, start_time, time_limit_ms);
                    board_data[x][y] = ' ';
                    
                    if (eval > maxEval) maxEval = eval;
                    if (eval > alpha) alpha = eval;
                    if (beta <= alpha) break;
                }
                return maxEval;
            } else {
                long long minEval = 100000000;
                for (auto [neg_score, x, y] : scored_moves) {
                    // Проверяем время реже для более глубокого анализа
                    static int time_check_counter = 0;
                    time_check_counter++;
                    if (time_check_counter % 15 == 0) { // Еще реже проверяем время
                        auto now = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                        if (elapsed > 4400) { // Используем 4 секунды из 4.5
                            break;
                        }
                    }
                    
                    if (!is_valid_move(x, y)) continue;
                    
                    make_move(x, y, opp_color);
                    long long eval = minimax(depth-1, true, my_color, opp_color, alpha, beta, start_time, time_limit_ms);
                    board_data[x][y] = ' ';
                    
                    if (eval < minEval) minEval = eval;
                    if (eval < beta) beta = eval;
                    if (beta <= alpha) break;
                }
                return minEval;
            }
        }
    
        // Итеративное углубление для поиска лучшего хода с ограничением времени
        std::pair<int, int> get_best_move(char player) {
            auto start_time = std::chrono::steady_clock::now();
            const int time_limit_ms = 4600; // 4.6 секунды для безопасности
            
            std::pair<int, int> fast = get_best_move_advanced(player);
             if(fast != std::pair<int, int>{-1, -1}){
                return fast;
             }

            // Быстрая оценка всех кандидатов для fallback
            auto moves = get_candidate_moves();
            std::pair<int, int> fallback_move = {-1, -1};
            int best_fallback_score = -1000000;
            for (auto [x, y] : moves) {
                if (is_valid_move(x, y)) {
                    int score = evaluate_simple_position(x, y, player);
                    if (score > best_fallback_score) {
                        best_fallback_score = score;
                        fallback_move = {x, y};
                    }
                }
            }

            // Итеративное углубление
            std::pair<int, int> best_move = fallback_move;
            long long best_score = best_fallback_score;
            for (int depth = 1; depth <= 12; ++depth) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                if (elapsed > 4400) {
                    // std::cerr << "Time limit reached at depth " << depth - 1 << ", using best found move" << std::endl;
                    break;
                }
                std::pair<int, int> current_best = {-1, -1};
                long long current_score = -100000000;
                std::vector<std::tuple<int, int, int>> scored_moves;
                for (auto [x, y] : moves) {
                    int score = evaluate_simple_position(x, y, player);
                    scored_moves.emplace_back(-score, x, y);
                }
                std::sort(scored_moves.begin(), scored_moves.end());
                int max_candidates = -3 + depth * 2;
                if (max_candidates < 8) max_candidates = 8;
                if (max_candidates > 20) max_candidates = 20;
                if (scored_moves.size() > max_candidates) scored_moves.resize(max_candidates);
                for (auto [neg_score, x, y] : scored_moves) {
                    static int time_check_counter = 0;
                    time_check_counter++;
                    if (time_check_counter % 10 == 0) {
                        auto now = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                        if (elapsed > 4400) {
                            // std::cerr << "Time limit reached during move evaluation" << std::endl;
                            break;
                        }
                    }
                    if (!is_valid_move(x, y)) continue;
                    make_move(x, y, player);
                    long long eval = minimax(depth-1, false, player, (player == 'B') ? 'W' : 'B', -100000000, 100000000, start_time, time_limit_ms);
                    board_data[x][y] = ' ';
                    if (eval > current_score) {
                        current_score = eval;
                        current_best = {x, y};
                    }
                }
                if (current_best.first != -1) {
                    if (current_score > best_score) {
                        best_move = current_best;
                        best_score = current_score;
                        // std::cerr << "Depth " << depth << " completed, NEW best move: " << best_move.first << "," << best_move.second << " score: " << best_score << " candidates: " << max_candidates << std::endl;
                    } else {
                        // std::cerr << "Depth " << depth << " completed, keeping previous best move: " << best_move.first << "," << best_move.second << " score: " << best_score << " (current: " << current_score << ") candidates: " << max_candidates << std::endl;
                    }
                }
            }
            if (best_move.first == -1) {
                // std::cerr << "Using fallback move: " << fallback_move.first << "," << fallback_move.second << std::endl;
                return fallback_move;
            }
            return best_move;
        }
        bool check_winning_move(int x, int y, char player) {
            board_data[x][y] = player;
            bool wins = check_win(x, y, player);
            board_data[x][y] = ' ';
            return wins;
        }
        int evaluate_simple_position(int x, int y, char player) {
            if (!is_valid_move(x, y)) return -1000;
            int score = 0;
            char opponent = (player == 'B') ? 'W' : 'B';
            int center_distance = abs(x - 15) + abs(y - 15);
            
            // Улучшенная оценка центра для белых
            if (player == 'W') {
                // Большой бонус за центр для белых
                if (x >= 12 && x <= 18 && y >= 12 && y <= 18) {
                    score += 500;
                }
                // Большой штраф за край доски для белых
                if (x <= 3 || x >= SIZE-4 || y <= 3 || y >= SIZE-4) {
                    score -= 300;
                }
                // Очень большой штраф за углы и края
                if (x <= 1 || x >= SIZE-2 || y <= 1 || y >= SIZE-2) {
                    score -= 800;
                }
                score += (30 - center_distance) * 12; // Очень большой бонус за близость к центру
            } else {
                score += (30 - center_distance) * 5;
            }
            
            // ОГРОМНЫЙ ШТРАФ за ходы в пустых областях
            bool has_nearby_stones = false;
            for (int dx = -2; dx <= 2; ++dx) {
                for (int dy = -2; dy <= 2; ++dy) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE && board_data[nx][ny] != ' ') {
                        has_nearby_stones = true;
                        break;
                    }
                }
            }
            if (!has_nearby_stones) {
                score -= 1000; // Уменьшенный штраф за ходы в пустоте
            }
            
            if (x == 0 || x == SIZE-1 || y == 0 || y == SIZE-1) score -= 50;
            
            // Улучшенная оценка атакующих возможностей
            for (int dir = 0; dir < 4; ++dir) {
                int count = 0, spaces = 0, blocked = 0;
                for (int i = 1; i <= 4; ++i) {
                    int nx = x + dx[dir] * i, ny = y + dy[dir] * i;
                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                    if (board_data[nx][ny] == player) count++;
                    else if (board_data[nx][ny] == ' ') spaces++;
                    else { blocked++; break; }
                }
                for (int i = 1; i <= 4; ++i) {
                    int nx = x + dx[dir + 4] * i, ny = y + dy[dir + 4] * i;
                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                    if (board_data[nx][ny] == player) count++;
                    else if (board_data[nx][ny] == ' ') spaces++;
                    else { blocked++; break; }
                }
                
                // Усиленная оценка атак
                if (count >= 4) score += 50000; // Четверка
                else if (count == 3 && spaces >= 1 && blocked == 0) score += 15000; // Открытая тройка
                else if (count == 3 && spaces >= 2) score += 8000; // Двойная открытая тройка
                else if (count == 2 && spaces >= 2 && blocked == 0) score += 3000; // Открытая двойка
                else if (count == 2 && spaces >= 3) score += 1000; // Двойка с пробелами
                else if (count == 1 && spaces >= 3) score += 200; // Одиночный камень
            }
            
            // Улучшенная оценка защитных ходов
            for (int dir = 0; dir < 4; ++dir) {
                int opp_count = 0, opp_spaces = 0, opp_blocked = 0;
                for (int i = 1; i <= 4; ++i) {
                    int nx = x + dx[dir] * i, ny = y + dy[dir] * i;
                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { opp_blocked++; break; }
                    if (board_data[nx][ny] == opponent) opp_count++;
                    else if (board_data[nx][ny] == ' ') opp_spaces++;
                    else { opp_blocked++; break; }
                }
                for (int i = 1; i <= 4; ++i) {
                    int nx = x + dx[dir + 4] * i, ny = y + dy[dir + 4] * i;
                    if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { opp_blocked++; break; }
                    if (board_data[nx][ny] == opponent) opp_count++;
                    else if (board_data[nx][ny] == ' ') opp_spaces++;
                    else { opp_blocked++; break; }
                }
                
                // Усиленная оценка блокировок
                if (opp_count >= 3 && opp_spaces >= 1) score += 25000; // Блокировка четверки
                else if (opp_count >= 2 && opp_spaces >= 2) score += 8000; // Блокировка тройки
                else if (opp_count >= 2 && opp_spaces >= 1) score += 3000; // Блокировка двойки
            }
            
            return score;
        }
        std::pair<int, int> get_random_move() {
            std::vector<std::pair<int, int>> moves;
            for (int i = 0; i < SIZE; i++)
                for (int j = 0; j < SIZE; j++)
                    if (is_valid_move(i, j)) moves.emplace_back(i, j);
            if (!moves.empty()) {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, moves.size() - 1);
                return moves[dis(gen)];
            }
            // std::cerr <<"Выводит -1 -1 ERROR";
            return {-1, -1};
        }
        // если 5+ подряд — победа
        bool check_win(int x, int y, char player) const {

            for (int dir = 0; dir < 4; ++dir) {
                int count = 1;
                // В одну сторону
                int nx = x + dx[dir], ny = y + dy[dir];
                while (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE && board_data[nx][ny] == player) {
                    count++;
                    nx += dx[dir];
                    ny += dy[dir];
                }
                // В другую сторону
                nx = x + dx[dir + 4], ny = y + dy[dir + 4];
                while (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE && board_data[nx][ny] == player) {
                    count++;
                    nx += dx[dir + 4];
                    ny += dy[dir + 4];
                }
                if(player=='B' &&( x ==15 && y==13)){
                    // std::cerr << std::endl <<"black ПОСЧИТАЛ "<< count;
                }
                if (count >= 5) return true;
            }
            return false;
        }
        
        // ищем пятёрку (быстрый перебор)
        std::pair<int, int> try4(char player) {
            for (int x = 0; x < SIZE; ++x) {
                for (int y = 0; y < SIZE; ++y) {
                    if (board_data[x][y] == ' ') {
                        board_data[x][y] = player;
                        bool win = check_win(x, y, player);
                        board_data[x][y] = ' ';
                        if (win) {
                            return {x, y};
                        }
                    }
                }
            }
            return {-1, -1};
        }
        
        // если надо заблокать пятёрку врага
        std::pair<int, int> try4_opponent(char player) {
            char opponent = (player == 'B') ? 'W' : 'B';
            return try4(opponent);
        }

        // ищем четвёрки (ну а вдруг)
        std::vector<std::pair<int, int>> find_fours(char player) {
            std::vector<std::pair<int, int>> fours;
            for (int x = 0; x < SIZE; ++x) {
                for (int y = 0; y < SIZE; ++y) {
                    if (board_data[x][y] == ' ') {
                        board_data[x][y] = player;
                        for (int dir = 0; dir < 4; ++dir) {
                            int count = 1, spaces = 0, blocked = 0;
                            for (int i = 1; i <= 4; ++i) {
                                int nx = x + dx[dir] * i, ny = y + dy[dir] * i;
                                if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                                if (board_data[nx][ny] == player) count++;
                                else if (board_data[nx][ny] == ' ') { spaces++; break; }
                                else { blocked++; break; }
                            }
                            for (int i = 1; i <= 4; ++i) {
                                int nx = x + dx[dir + 4] * i, ny = y + dy[dir + 4] * i;
                                if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                                if (board_data[nx][ny] == player) count++;
                                else if (board_data[nx][ny] == ' ') { spaces++; break; }
                                else { blocked++; break; }
                            }
                            if (count >= 4 && spaces >= 1) {
                                fours.emplace_back(x, y);
                                break;
                            }
                        }
                        board_data[x][y] = ' ';
                    }
                }
            }
            return fours;
        }
        // ищем тройки
        std::vector<std::pair<int, int>> find_threes(char player) {
            std::vector<std::pair<int, int>> threes;
            for (int x = 0; x < SIZE; ++x) {
                for (int y = 0; y < SIZE; ++y) {
                    if (board_data[x][y] == ' ') {
                        board_data[x][y] = player;
                        for (int dir = 0; dir < 4; ++dir) {
                            int count = 1, spaces = 0;
                            for (int i = 1; i <= 4; ++i) {
                                int nx = x + dx[dir] * i, ny = y + dy[dir] * i;
                                if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                if (board_data[nx][ny] == player) count++;
                                else if (board_data[nx][ny] == ' ') { spaces++; break; }
                                else break;
                            }
                            for (int i = 1; i <= 4; ++i) {
                                int nx = x + dx[dir + 4] * i, ny = y + dy[dir + 4] * i;
                                if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) break;
                                if (board_data[nx][ny] == player) count++;
                                else if (board_data[nx][ny] == ' ') { spaces++; break; }
                                else break;
                            }
                            if (count >= 3 && spaces >= 1) {
                                threes.emplace_back(x, y);
                                break;
                            }
                        }
                        board_data[x][y] = ' ';
                    }
                }
            }
            return threes;
        }
        
        
        // Очень быстрая функция для простых случаев
        std::pair<int, int> get_quick_move(char player) {
            // 1. Победа в 1 ход
            auto win5 = try4(player);
            if (win5.first != -1) return win5;

            // 2. Блокировка победы соперника
            auto block5 = try4_opponent(player);
            if (block5.first != -1) return block5;

            // 3. Лучший по быстрой оценке
            auto moves = get_candidate_moves();
            std::pair<int, int> best_move = {-1, -1};
            int best_score = -1000000;
            
            for (auto [x, y] : moves) {
                if (is_valid_move(x, y)) {
                    int score = evaluate_simple_position(x, y, player);
                    if (score > best_score) {
                        best_score = score;
                        best_move = {x, y};
                    }
                }
            }
            return best_move;
        }
        
        // Поиск двойных угроз 
        std::vector<std::pair<int, int>> find_double_threats(char player) {
            std::vector<std::pair<int, int>> double_threats;
            
            // Проверяем все свободные клетки
            for (int x = 0; x < SIZE; ++x) {
                for (int y = 0; y < SIZE; ++y) {
                    if (board_data[x][y] != ' ') continue;
                    
                    // Временно ставим камень соперника
                    board_data[x][y] = player;
                    
                    // Считаем количество угроз (троек и четверок)
                    int threat_count = 0;
                    
                    // Проверяем все направления
                    for (int dir = 0; dir < 4; ++dir) {
                        int count = 1, spaces = 0, blocked = 0;
                        
                        // В одну сторону
                        for (int i = 1; i <= 4; ++i) {
                            int nx = x + dx[dir] * i, ny = y + dy[dir] * i;
                            if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                            if (board_data[nx][ny] == player) count++;
                            else if (board_data[nx][ny] == ' ') { spaces++; break; }
                            else { blocked++; break; }
                        }
                        
                        // В противоположную сторону
                        for (int i = 1; i <= 4; ++i) {
                            int nx = x + dx[dir + 4] * i, ny = y + dy[dir + 4] * i;
                            if (nx < 0 || nx >= SIZE || ny < 0 || ny >= SIZE) { blocked++; break; }
                            if (board_data[nx][ny] == player) count++;
                            else if (board_data[nx][ny] == ' ') { spaces++; break; }
                            else { blocked++; break; }
                        }
                        
                        // Считаем угрозы
                        if (count >= 4 && spaces >= 1) threat_count++; // Четверка
                        else if (count >= 3 && spaces >= 2 && blocked == 0) threat_count++; // Открытая тройка
                    }
                    
                    // Убираем камень
                    board_data[x][y] = ' ';
                    
                    // Если найдено 2 или более угроз - это двойная угроза
                    if (threat_count >= 2) {
                        double_threats.emplace_back(x, y);
                    }
                }
            }
            
            return double_threats;
        }


        std::pair<int, int> get_best_move_advanced(char player) {
            char opponent = (player == 'B') ? 'W' : 'B';
            // если есть пятёрка — сразу ставим
            auto win5 = try4(player);
            if (win5.first != -1) {
                return win5;
            }
            // блочим пятёрку врага
            auto block5 = try4_opponent(player);
            if (block5.first != -1) {
                return block5;
            }
            // ищем свою четвёрку
            auto my_fours = find_fours(player);
            if (!my_fours.empty()) {
                return my_fours[0];
            }
            // блочим четвёрку врага
            auto opp_fours = find_fours(opponent);
            if (!opp_fours.empty()) {
                return opp_fours[0];
            }
            // ищем тройку
            auto my_threes = find_threes(player);
            if (!my_threes.empty()) {
                return my_threes[0];
            }
            // блочим тройку врага
            auto opp_threes = find_threes(opponent);
            if (!opp_threes.empty()) {
                return opp_threes[0];
            }
            // ищем двойную угрозу у врага (две тройки/четвёрки)
            auto double_threats = find_double_threats(opponent);
            if (!double_threats.empty()) {
                return double_threats[0];
            }
            // если ничего критичного — дальше обычный поиск
            return {-1, -1};
        }
};