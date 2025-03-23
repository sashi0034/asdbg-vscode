class Player {
    int life;
    double x, y;

    void move(double dx, double dy) {
        x += dx;
        y += dy;
    }
}

void main() {
    int player_life = 10;
    Player p = Player();
    p.life = player_life;
    p.x = 0;
    p.y = 0;
    p.move(1, 2);
}
