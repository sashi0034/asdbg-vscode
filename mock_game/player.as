class Player {
    int life;
    double x, y;

    void move(double dx, double dy) {
        x += dx;
        y += dy;
    }
}

// This is a mock program.
void main() {
    int initial_player_life = 10;
    int player_damage = 3;
    int player_life = initial_player_life + player_damage; // player_life

    Player p = Player();
    p.life = player_life;
    p.x = 0;
    p.y = 0;
    p.move(1, 2);
}

int getInterestingValue() {
    return 51;
}
