#include "SFML/Graphics.hpp"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <queue>
#include <mutex>
#include <semaphore.h>

using namespace std;
using namespace sf;

RenderWindow window(sf::VideoMode(1000, 864), "Pacman");

Vector2f scalingFactor(2,2);
Vector2i playerLastPing;
const int mapSize = 27;
int mapArray[mapSize][mapSize];
float dt;
float powerPallet = 15;
int score = 0;
bool arrayKeysKhali[4];

sem_t playerPath;
sem_t key;
sem_t permit;
sem_t SpeedGhost;
int noTextures = 19;
sf::Sprite *mapSprites = new sf::Sprite[noTextures];

mutex mtx;
int curr_power_pellets = 4;
int max_power_pellets = 4;
queue<Vector2i> location;

// Function to check valid Move
bool canMove(int x, int y)
{
    return (mapArray[y][x] == 1) || (mapArray[y][x] == 14) || (mapArray[y][x] == 15) || (mapArray[y][x] == 16) || (mapArray[y][x] == 17);
}


struct Player
{
    Vector2i pos;
    Vector2i targetPos;
    int dir;
    int targetDir;
    float speed;
    Sprite sprite;
    float plTimer;
    Clock plClock;
    vector<Vector2i> path;
    bool powerMode;
    Clock powerClock;
    float powerDuration;

    Player() {}

    void initPlayer(Vector2i pos, Vector2i target_pos, int dir, int target_dir, float speed, Texture &texture)
    {
        this->pos = pos;
        this->dir = dir;
        this->targetPos = target_pos;
        this->targetDir = target_dir;
        this->speed = speed;
        this->sprite.setTexture(texture);
        this->sprite.setPosition(pos.x * 32, pos.y * 32);
        this->path.push_back(pos);
        playerLastPing = pos;
        powerMode = false;
        powerDuration = 10.0f;
    }

    void playerMove()
    {
        if (mapArray[pos.y][pos.x] == 14)
        {
            score++;
            mapArray[pos.y][pos.x] = 1;
        }
        if (pos.x == 26 && dir == 4)
        {
            pos.x = 0;
            sprite.setPosition(0, sprite.getPosition().y);
            return;
        }
        else if (pos.x == 0 && dir == 3)
        {
            pos.x = 26;
            sprite.setPosition(26 * 32, sprite.getPosition().y);
            return;
        }
        if (canMove(pos.x + targetPos.x, pos.y + targetPos.y))
        {
            dir = targetDir;
        }

        //   FOR POWER-PELLET
        if (mapArray[pos.y][pos.x] == 15 && !powerMode)
        {
            score += 50;
            mapArray[pos.y][pos.x] = 1;
            powerMode = true;
            powerClock.restart();

            mtx.lock();
            curr_power_pellets--;
            location.push(pos);
            mtx.unlock();
        }

        switch (dir)
        {
        case 1:
            if (canMove(pos.x, pos.y - 1))
            {
                if (sprite.getPosition().y / 32 < pos.y - 1)
                    pos.y--;
                sprite.move(0, speed * -plTimer);
            }
            break;
        case 2:
            if (canMove(pos.x, pos.y + 1))
            {
                if (sprite.getPosition().y / 32 > pos.y + 1)
                    pos.y++;
                sprite.move(0, speed * plTimer);
            }
            break;
        case 3:
            if (canMove(pos.x - 1, pos.y))
            {
                if (sprite.getPosition().x / 32 < pos.x - 1)
                    pos.x--;
                sprite.move(speed * -plTimer, 0);
            }
            break;
        case 4:
            if (canMove(pos.x + 1, pos.y))
            {
                if (sprite.getPosition().x / 32 > pos.x + 1)
                    pos.x++;
                sprite.move(speed * plTimer, 0);
            }
            break;
        default:
            break;
        }
    }

    void updatePowerMode()
    {
        if (powerMode && powerClock.getElapsedTime().asSeconds() >= powerDuration)
        {
            powerMode = false;
        }
    }

    void setTarget(Keyboard::Key key)
    {
        if (Keyboard::isKeyPressed(Keyboard::W))
        {
            targetPos.x = 0;
            targetPos.y = -1;
            targetDir = 1;
        }
        if (Keyboard::isKeyPressed(Keyboard::S))
        {
            targetPos.x = 0;
            targetPos.y = 1;
            targetDir = 2;
        }
        if (Keyboard::isKeyPressed(Keyboard::A))
        {
            targetPos.x = -1;
            targetPos.y = 0;
            targetDir = 3;
        }
        if (Keyboard::isKeyPressed(Keyboard::D))
        {
            targetPos.x = 1;
            targetPos.y = 0;
            targetDir = 4;
        }
    }

    void killPlayer()
    {
        sprite.setPosition(13 * 32, 23 * 32);
        targetDir = 0;
        targetPos.x = 0;
        targetPos.y = 0;
        pos.x = 13;
        pos.y = 23;
        dir = 0;
    }
};

struct Ghosts
{
    Vector2i pos;
    Vector2i targetPos;
    int dir;
    int targetDir;
    float speed;
    Sprite sprite;
    float gtimer;
    Clock gtClock;
    bool keyFlag;
    bool permitFlag;
    bool frightened;
    Clock frightenedClock;
    float frightenedDuration;
    Texture normalTexture;
    Texture frightenedTexture;

    Ghosts() : frightened(false), frightenedDuration(10.0f), dir(0), targetDir(0), keyFlag(false), permitFlag(false) {}

    void initGhost(Vector2i pos, Texture &normalTexture, Texture &frightenedTexture)
    {
        this->pos = pos;
        this->normalTexture = normalTexture;
        this->frightenedTexture = frightenedTexture;
        this->sprite.setTexture(normalTexture);
        this->sprite.setPosition(pos.x * 32, pos.y * 32);
        this->speed = 100.f;
    }

    bool GateCheck(int x, int y)
    {
        if (mapArray[y][x] == 18)
        {
            return keyFlag && permitFlag;
        }
        return false;
    }

    void moveGhost()
    {
        if (frightened)
        {
            sprite.setColor(Color::Blue);
        }
        else
        {
            sprite.setColor(Color::White);
        }
        if (pos.y > targetPos.y)
        {
            dir = 1;
        }
        else if (pos.y < targetPos.y)
        {
            dir = 2;
        }
        else if (pos.x > targetPos.x)
        {
            dir = 3;
        }
        else if (pos.x < targetPos.x)
        {
            dir = 4;
        }

        switch (dir)
        {
        case 1:
            if (canMove(pos.x, pos.y - 1) || GateCheck(pos.x, pos.y - 1))
            {
                if (sprite.getPosition().y / 32 < pos.y - 0.5)
                    pos.y--;
                sprite.move(0, speed * -gtimer);
            }
            break;
        case 2:
            if (canMove(pos.x, pos.y + 1) || GateCheck(pos.x, pos.y + 1))
            {
                if (sprite.getPosition().y / 32 > pos.y + 0.5)
                    pos.y++;
                sprite.move(0, speed * gtimer);
            }
            break;
        case 3:
            if (canMove(pos.x - 1, pos.y) || GateCheck(pos.x - 1, pos.y))
            {
                if (sprite.getPosition().x / 32 < pos.x - 0.5)
                    pos.x--;
                sprite.move(speed * -gtimer, 0);
            }
            break;
        case 4:
            if (canMove(pos.x + 1, pos.y) || GateCheck(pos.x + 1, pos.y))
            {
                if (sprite.getPosition().x / 32 > pos.x + 0.5)
                    pos.x++;
                sprite.move(speed * gtimer, 0);
            }
            break;
        default:
            break;
        }
    }

    void updateFrightenedMode()
    {
        if (frightened && frightenedClock.getElapsedTime().asSeconds() >= frightenedDuration)
        {
            frightened = false;
        }
    }

    void resetPosition()
    {
        pos.x = 13;
        pos.y = 13;
        sprite.setPosition(pos.x * 32, pos.y * 32);
    }

    bool CheckFreed()
    {
        return keyFlag && permitFlag;
    }
};

const int numGhosts = 4;
Ghosts ghosts[4];
Player pacman;

// Reader Writer
void *Trail(void *arg)
{
    int type = *(int *)arg;
    if (type == 0)
    {
        while (window.isOpen())
        {
            sem_wait(&playerPath);
            if (pacman.path.back() != pacman.pos)
                pacman.path.push_back(pacman.pos);
            sem_post(&playerPath);
        }
    }
    else
    {
        while (window.isOpen())
        {
            sem_wait(&playerPath);
            if (!pacman.path.empty())
            {
                playerLastPing = pacman.path.front();
                pacman.path.erase(pacman.path.begin());
            }

            sem_post(&playerPath);
        }
    }
    return NULL;
}

// Dinning Philosipher and Producer Consumer
void *consume(void *arg)
{
    int i = *(int *)arg;
    Vector2i keysPos;
    Vector2i PermitsPos;
    while (true && !ghosts[i].CheckFreed())
    {
        if (!arrayKeysKhali[i] && mapArray[ghosts[i].pos.y][ghosts[i].pos.x] == 16)
        {
            sem_wait(&key);
            cout << "Obtained KEy " << i << endl;
            ghosts[i].sprite.setColor(Color::Red);
            mapArray[ghosts[i].pos.y][ghosts[i].pos.x] = 1;
            keysPos = ghosts[i].pos;
            ghosts[i].keyFlag = true;
            arrayKeysKhali[i] = true;
        }
        if (arrayKeysKhali[i] && mapArray[ghosts[i].pos.y][ghosts[i].pos.x] == 17)
        {
            cout << "Permit Seen" << i << endl;
            sem_wait(&permit);
            cout << "Obtained Permit " << i << endl;
            mapArray[ghosts[i].pos.y][ghosts[i].pos.x] = 1;
            PermitsPos = ghosts[i].pos;
            ghosts[i].permitFlag = true;
            sem_post(&permit);
            mapArray[PermitsPos.y][PermitsPos.x] = 17;
            sem_post(&key);
            mapArray[keysPos.y][keysPos.x] = 16;
        }
    }
    return NULL;
}

// Speed Boost Problem
void *SpeedUp(void *arg)
{
    int idx = *(int *)arg;
    Clock clockSpeed;
    float dtSpeed = 0;
    while (window.isOpen())
    {
        sem_wait(&SpeedGhost);
        
            ghosts[idx].speed = 150.f;
        while (dtSpeed < 10000.f)
        {
            dtSpeed += 0.00001f;
        }
        cout<<idx<<" "<<ghosts[idx].speed<<endl;
        dtSpeed = 0;
        ghosts[idx].speed = 100.f;
        sem_post(&SpeedGhost);
    }
     return NULL;

}

// Function to find the shortest path using Breadth-First Search (BFS)
Vector2i findShortestPath(Vector2i start, Vector2i end, int idx)
{
    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};
    bool visited[mapSize][mapSize] = {false};
    queue<Vector2i> q;
    Vector2i parent[mapSize][mapSize];
    Vector2i child;
    q.push(start);
    visited[start.x][start.y] = true;
    while (!q.empty())
    {
        Vector2i curr = q.front();
        q.pop();

        if (curr == end)
        {

            while (!(curr.x == start.x && curr.y == start.y))
            {
                child = curr;
                curr = {parent[curr.x][curr.y].x, parent[curr.x][curr.y].y};
            }

            return child;
        }

        for (int i = 0; i < 4; ++i)
        {
            int nx = curr.x + dx[i];
            int ny = curr.y + dy[i];
            if ((canMove(nx, ny) || ghosts[idx].GateCheck(nx, ny)) && !visited[nx][ny])
            {
                visited[nx][ny] = true;
                parent[nx][ny] = {curr.x, curr.y};
                q.push({nx, ny});
            }
        }
    }
    return start;
}

// Thread function to handle the ghost movement
void *ghostMovement(void *arg)
{
    int ghostIndex = *((int *)arg);

    int *i = new int;
    *i = (ghostIndex + 1);
    pthread_t pathTarget;
    pthread_create(&pathTarget, NULL, Trail, (void *)(i));
    Clock timerClock;
    pthread_t keyPermit;
    pthread_t spGhostId;
    int *idx = new int;
    *idx = ghostIndex;
    pthread_create(&keyPermit, NULL, consume, (void *)(idx));
    pthread_create(&spGhostId, NULL, SpeedUp, (void *)(idx));

    float timer = timerClock.restart().asSeconds();
    while (window.isOpen())
    {
        ghosts[ghostIndex].gtimer = ghosts[ghostIndex].gtClock.restart().asSeconds();
        if (ghosts[ghostIndex].CheckFreed())
        {
            // Calculate the shortest path to the player's position using BFS
            Vector2i ghostTarget = findShortestPath(ghosts[ghostIndex].pos, playerLastPing, ghostIndex);
            ghosts[ghostIndex].targetPos = ghostTarget;
        }
        else
        {
            if (timer > 200.f)
            {
                timer = 0;
                int randomX = rand() % 7 + 10;
                int randomY = rand() % 5 + 10;
                ghosts[ghostIndex].targetPos.x = randomX;
                ghosts[ghostIndex].targetPos.y = randomY;
            }
            timer += 0.0001;
        }

        ghosts[ghostIndex].moveGhost();
    }
    return NULL;
}

// Thread function to handle the player movement
void *playerManagement(void *arg)
{
    pthread_t pathid;
    int *i = new int;
    *i = 0;
    pthread_create(&pathid, NULL, Trail, (void *)i);
    while (window.isOpen())
    {
        pacman.plTimer = pacman.plClock.restart().asSeconds();
        pacman.playerMove();
    }
    return NULL;
}

// Load map textures
void loadMapTextures(sf::Texture *&texts, int size)
{
    texts[0].loadFromFile("sprites/map/0.png");
    texts[1].loadFromFile("sprites/map/1.png");
    texts[14].loadFromFile("sprites/map/14.png");
    texts[15].loadFromFile("sprites/map/15.png");
    texts[16].loadFromFile("sprites/map/16.png");
    texts[17].loadFromFile("sprites/map/17.png");
    texts[18].loadFromFile("sprites/map/18.png");
}

int main()
{
    srand(time(0));
    window.setFramerateLimit(120);

    sem_init(&playerPath, 0, 1);
    sem_init(&key, 0, 2);
    sem_init(&permit, 0, 2);
    sem_init(&SpeedGhost, 0, 2);

    // Map textures
    int noTextures = 19;
    sf::Texture *mapText = new sf::Texture[noTextures];
    loadMapTextures(mapText, noTextures);

    sf::Sprite *mapSprites = new sf::Sprite[noTextures];
    for (int i = 0; i < noTextures; i++)
    {
        mapSprites[i].setTexture(mapText[i]);
        mapSprites[i].setScale(scalingFactor);
    }

    std::ifstream ifs("maps/map1.txt");
    if (ifs.is_open())
    {
        char a, b;
        for (int i = 0; i < mapSize; i++)
        {
            for (int j = 0; j < mapSize; j++)
            {
                ifs.get(a);
                if (a == '\n' || a == ' ')
                    j--;
                else
                {
                    ifs.get(b);
                    mapArray[i][j] = (a - '0') * 10 + (b - '0');
                }
            }
        }
    }
    ifs.close();

    // Create ghosts
    pthread_t tid[numGhosts];
    Texture ghostTexture;
    ghostTexture.loadFromFile("sprites/str.png");
    Texture ghostFrightenedTexture;
    ghostFrightenedTexture.loadFromFile("sprites/ghostblue.png");
    for (int i = 0; i < numGhosts; i++)
    {
        ghosts[i].initGhost(Vector2i(13 + i, 13), ghostTexture, ghostFrightenedTexture);
        ghosts[i].sprite.setScale(scalingFactor);
        int *ghostIndex = new int(i);
        pthread_create(&tid[i], NULL, ghostMovement, (void *)ghostIndex);
    }

    // Player (Pacman) setup
    Texture pacmanTexture;
    pacmanTexture.loadFromFile("sprites/player.png");
    pacman.initPlayer(Vector2i(13, 23), Vector2i(0, 0), 0, 0, 100.f, pacmanTexture);
    pacman.sprite.setScale(scalingFactor);
    pthread_t playerThreadID;
    pthread_create(&playerThreadID, NULL, playerManagement, NULL);

    int lives = 3;

    Font font;
    font.loadFromFile("sprites/Arial.ttf");
    Text livesText;
    Text scoreText;
    livesText.setFont(font);
    livesText.setCharacterSize(24);
    livesText.setFillColor(Color::White);
    livesText.setPosition(880, 10);

    scoreText.setFont(font);
    scoreText.setCharacterSize(24);
    scoreText.setFillColor(Color::White);
    scoreText.setPosition(880, 40);

    // Main loop
    sf::Clock dtClock;
    while (window.isOpen() && lives >= 0)
    {

        if (!pacman.powerMode)
        {
            for (int i = 0; i < numGhosts; ++i)
            {
                ghosts[i].frightened = false; // Set all ghosts to not frightened
            }
        }
        mtx.lock();
        if (!location.empty() && curr_power_pellets == 0)
        {
            Vector2i currentPos = location.front();
            location.pop();
            cout << "y: " << currentPos.y << "x: " << currentPos.y;
            if (mapArray[currentPos.y][currentPos.x] == 1)
            {
                // Respawn a power pellet at the current position
                curr_power_pellets++;
                mapArray[currentPos.y][currentPos.x] = 15;
            }
        }
        mtx.unlock();

        dt = dtClock.restart().asSeconds();

        pacman.updatePowerMode();
        for (int i = 0; i < numGhosts; ++i)
        {
            ghosts[i].updateFrightenedMode();
        }

        if (pacman.powerMode)
        {
            for (int i = 0; i < numGhosts; ++i)
            {
                ghosts[i].frightened = true;
                ghosts[i].frightenedClock.restart();
            }
        }

        sf::Event sfEvent;
        while (window.pollEvent(sfEvent))
        {
            if (sfEvent.type == sf::Event::Closed)
                window.close();
            if (sfEvent.type == sf::Event::KeyPressed)
            {
                if (sfEvent.key.code == sf::Keyboard::Escape)
                    window.close();

                pacman.setTarget(sfEvent.key.code);
            }
        }

        // Check collisions with ghosts
        for (size_t i = 0; i < numGhosts; ++i)
        {
            if (pacman.sprite.getGlobalBounds().intersects(ghosts[i].sprite.getGlobalBounds()))
            {
                // During Power Up
                if (pacman.powerMode && ghosts[i].frightened)
                {
                    ghosts[i].resetPosition();

                    score += 20;
                }
                // Normal Case
                else
                {
                    lives--;
                    pacman.killPlayer();
                }
            }
        }

        /////////////////////////////////Render////////////////////////////

        window.clear();

        for (int i = 0; i < mapSize; i++)
        {
            for (int j = 0; j < mapSize; j++)
            {
                if (mapArray[i][j] == 14 || mapArray[i][j] == 16 || mapArray[i][j] == 17 || mapArray[i][j] == 18)
                {
                    mapSprites[1].setPosition(j * 32, i * 32);
                    window.draw(mapSprites[1]);
                }
                else if (mapArray[i][j] == 15)
                {
                    mapSprites[1].setPosition(j * 32, i * 32);
                    window.draw(mapSprites[1]);
                }
                mapSprites[mapArray[i][j]].setPosition(j * 32, i * 32);
                window.draw(mapSprites[mapArray[i][j]]);
            }
        }

        window.draw(pacman.sprite);

        for (int i = 0; i < numGhosts; ++i)
        {
            window.draw(ghosts[i].sprite);
        }

        // Draw lives
        livesText.setString("Lives: " + to_string(lives));
        scoreText.setString("Score: " + to_string(score));
        window.draw(livesText);
        window.draw(scoreText);

        window.display();
    }

    return 0;
}