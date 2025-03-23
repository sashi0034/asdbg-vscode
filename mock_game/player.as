class Player {
    int hp;
    double x, y;

    void move(double dx, double dy) {
        x += dx;
        y += dy;
    }
}

void main() {
    Player p = Player();
    p.hp = 10;
    p.x = 0;
    p.y = 0;
    p.move(1, 2);
}
