// MultiplayerSnake.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <SDL.h>
#include <iostream>
#include <string>

#include <vector>
#include "SDL2net\include\SDL_net.h"
#include <SDL_ttf.h>
#include <sstream>
const int SCREEN_WIDTH = 1000;
const int SCREEN_HEIGHT = 1000;
const int GRID_DIVIDER = 2;
const int FPS = 60;
const int SCREEN_TICKS_PER_FRAME = 1000 / FPS;


const int gridHeight = 50;
const int gridWidth = 50;
using namespace std;

struct vec2
{
	int x;
	int y;
	vec2() {};
	vec2(int x, int y) : x(x), y(y) {};

	friend inline bool operator==(const vec2 lhs, const vec2 rhs) { return(lhs.x == rhs.x && lhs.y == rhs.y); }
};

enum constantDirection
{
	UP,
	DOWN,
	LEFT,
	RIGHT
};
enum dataType
{
	NONE,
	BODY,
	BIP,
	SERVER
};

int grid[gridHeight][gridWidth];

#define PORT 2000

struct packet
{
	int id;
	dataType dataType = NONE;
	int bodylength;
	vec2 body[100];
};

struct handshake
{
	string st = "hello";
	int clientIp;
};

struct player
{
	handshake hs;
	std::vector<vec2> playerBody;
};

vec2 playerLocation;
std::vector<vec2> playerBody;
vec2 bipLocation;

constantDirection playerDirection = UP;

//The application time based timer
class LTimer
{
public:
	//Initializes variables
	LTimer();

	//The various clock actions
	void start();
	void stop();
	void pause();
	void unpause();

	//Gets the timer's time
	Uint32 getTicks();

	//Checks the status of the timer
	bool isStarted();
	bool isPaused();

private:
	//The clock time when the timer started
	Uint32 mStartTicks;

	//The ticks stored when the timer was paused
	Uint32 mPausedTicks;

	//The timer status
	bool mPaused;
	bool mStarted;
};

LTimer::LTimer()
{
	//Initialize the variables
	mStartTicks = 0;
	mPausedTicks = 0;

	mPaused = false;
	mStarted = false;
}

void LTimer::start()
{
	//Start the timer
	mStarted = true;

	//Unpause the timer
	mPaused = false;

	//Get the current clock time
	mStartTicks = SDL_GetTicks();
	mPausedTicks = 0;
}

void LTimer::stop()
{
	//Stop the timer
	mStarted = false;

	//Unpause the timer
	mPaused = false;

	//Clear tick variables
	mStartTicks = 0;
	mPausedTicks = 0;
}

void LTimer::pause()
{
	//If the timer is running and isn't already paused
	if (mStarted && !mPaused)
	{
		//Pause the timer
		mPaused = true;

		//Calculate the paused ticks
		mPausedTicks = SDL_GetTicks() - mStartTicks;
		mStartTicks = 0;
	}
}

void LTimer::unpause()
{
	//If the timer is running and paused
	if (mStarted && mPaused)
	{
		//Unpause the timer
		mPaused = false;

		//Reset the starting ticks
		mStartTicks = SDL_GetTicks() - mPausedTicks;

		//Reset the paused ticks
		mPausedTicks = 0;
	}
}

Uint32 LTimer::getTicks()
{
	//The actual timer time
	Uint32 time = 0;

	//If the timer is running
	if (mStarted)
	{
		//If the timer is paused
		if (mPaused)
		{
			//Return the number of ticks when the timer was paused
			time = mPausedTicks;
		}
		else
		{
			//Return the current time minus the start time
			time = SDL_GetTicks() - mStartTicks;
		}
	}

	return time;
}

bool LTimer::isStarted()
{
	//Timer is running and paused or unpaused
	return mStarted;
}

bool LTimer::isPaused()
{
	//Timer is running and paused
	return mPaused && mStarted;
}

/**
* Log an SDL error with some error message to the output stream of our choice
* @param os The output stream to write the message to
* @param msg The error message to write, format will be msg error: SDL_GetError()
*/
void logSDLError(std::ostream &os, const std::string &msg) {
	os << msg.c_str() << " error: " << SDL_GetError() << std::endl;
}

/**
* Loads a BMP image into a texture on the rendering device
* @param file The BMP image file to load
* @param ren The renderer to load the texture onto
* @return the loaded texture, or nullptr if something went wrong.
*/
SDL_Texture* loadTexture(const std::string &file, SDL_Renderer *ren) {
	//Initialize to nullptr to avoid dangling pointer issues
	SDL_Texture *texture = nullptr;
	//Load the image
	SDL_Surface *loadedImage = SDL_LoadBMP(file.c_str());
	//If the loading went ok, convert to texture and return the texture
	if (loadedImage != nullptr) {
		texture = SDL_CreateTextureFromSurface(ren, loadedImage);
		SDL_FreeSurface(loadedImage);
		//Make sure converting went ok too
		if (texture == nullptr) {
			logSDLError(std::cout, "CreateTextureFromSurface");
		}
	}
	else {
		logSDLError(std::cout, "LoadBMP");
	}
	return texture;
}

/**
* Draw an SDL_Texture to an SDL_Renderer at position x, y, preserving
* the texture's width and height
* @param tex The source texture we want to draw
* @param ren The renderer we want to draw to
* @param x The x coordinate to draw to
* @param y The y coordinate to draw to
*/
void renderTexture(SDL_Texture *tex, SDL_Renderer *ren, int x, int y) {
	//Setup the destination rectangle to be at the position we want
	SDL_Rect dst;
	dst.x = x;
	dst.y = y;
	//Query the texture to get its width and height to use
	SDL_QueryTexture(tex, NULL, NULL, &dst.w, &dst.h);
	SDL_RenderCopy(ren, tex, NULL, &dst);
}

SDL_Window *win;
SDL_Renderer *ren;
SDL_Texture *gridBackground;
SDL_Texture *gridPlayer;
SDL_Texture *gridBip;
std::vector<player> otherplayers;

//Timers
LTimer fpsTimer; //The frames per second timer
LTimer capTimer; //The frames per second cap timer
int countedFrames = 0;

int init()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		logSDLError(std::cout, "SDL_Init");
		return 1;
	}

	win = SDL_CreateWindow("MultiplayerSnake", 900, 100, SCREEN_WIDTH,
		SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	if (win == nullptr) {
		logSDLError(std::cout, "CreateWindow");
		SDL_Quit();
		return 1;
	}
	ren = SDL_CreateRenderer(win, -1,
		SDL_RENDERER_ACCELERATED);
	if (ren == nullptr) {
		logSDLError(std::cout, "CreateRenderer");
		SDL_DestroyWindow(win);
		SDL_Quit();
		return 1;
	}


	//In memory text stream
	stringstream timeText;
	//Start counting frames per second
	fpsTimer.start();

	std::string gridPath = "Grid.bmp";
	std::string DotPath = "Dot.bmp";
	std::string BipPath = "Bip.bmp";
	gridBackground = loadTexture(gridPath, ren);
	gridPlayer = loadTexture(DotPath, ren);
	gridBip = loadTexture(BipPath, ren);
	if (gridBackground == nullptr || gridPlayer == nullptr) {
		//cleanup(background, image, renderer, win);
		SDL_Quit();
		return 1;
	}

	SDL_SetWindowSize(win, gridHeight * 10, gridHeight * 10);

	//init
	for (size_t i = 0; i < gridHeight; i++)
	{
		for (size_t j = 0; j < gridWidth; j++)
		{
			grid[i][j] = 0;
		}
	}
	playerLocation = vec2(23, 23);
	playerBody.push_back(playerLocation);
	bipLocation = vec2(23, 20);
	return 1;
}

SDL_Event e;
bool quit = false;
int Input()
{
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) {
			quit = true;
		}
		//If user presses esc
		if (e.key.keysym.sym == SDLK_ESCAPE) {
			quit = true;
		}

		//playerinput
		if (e.key.state == SDL_PRESSED)
		{
			if (e.key.keysym.sym == SDLK_UP)
				playerDirection = UP;

			if (e.key.keysym.sym == SDLK_DOWN)
				playerDirection = DOWN;

			if (e.key.keysym.sym == SDLK_LEFT)
				playerDirection = LEFT;

			if (e.key.keysym.sym == SDLK_RIGHT)
				playerDirection = RIGHT;
		}
	}
	return 1;

}
int Update()
{
	return 1;
}
int Draw()
{
	//Render our scene
	SDL_RenderClear(ren);

	int bW, bH;
	SDL_QueryTexture(gridBackground, NULL, NULL, &bW, &bH);
	//renderTexture(background, ren, 0, 0);
	//renderTexture(background, ren, bW, 0);
	//renderTexture(background, ren, 0, bH);
	//renderTexture(background, ren, bW, bH);
	for (size_t i = 0; i < gridHeight; i++)
	{
		for (size_t j = 0; j < gridHeight; j++)
		{
			renderTexture(gridBackground, ren, i*bW, j*bW);
		}
	}

	for each (vec2 var in playerBody)
	{
		renderTexture(gridPlayer, ren, var.x * bW, var.y*bW);
	}

	for each (player var in otherplayers)
	{
		for each (vec2 var2 in var.playerBody)
		{
			renderTexture(gridPlayer, ren, var2.x * bW, var2.y*bW);
		}
	}

	renderTexture(gridBip, ren, bipLocation.x * bW, bipLocation.y*bW);


	int iW, iH;
	SDL_QueryTexture(gridPlayer, NULL, NULL, &iW, &iH);
	int x = SCREEN_WIDTH / 2 - iW / 2;
	int y = SCREEN_HEIGHT / 2 - iH / 2;
	//renderTexture(image, ren, x, y);

	SDL_RenderPresent(ren);

	return 1;
}


int main(int, char**) {
	
	init();

	//SERVER

	bool isServer = false;
	TCPsocket server = 0;
	TCPsocket client;
	vector<TCPsocket> clients;
	float serverTickRate = 60;
	float serverTickTimer = -1;
	SDLNet_Init();

	//CLIENT
	IPaddress ip; //90.253.170.185
	SDLNet_ResolveHost(&ip, "127.0.0.1", PORT);
	server = SDLNet_TCP_Open(&ip);
	handshake hs;
	
	if (server)
	{
		int ret = SDLNet_TCP_Recv(server, &hs, sizeof(handshake));
		cout << "handshake :" << sizeof(handshake) << endl;
		cout << "Received :" << ret << endl;
		std::cout << "Connected to " << SDLNet_TCP_GetPeerAddress(server)->host << std::endl;

		std::cout << hs.st << std::endl;
	}
	else
	{
		cout << "No Server was detected, would you like to create one?" << endl;
		char res;
		//cin >> res;
		if (1)
		{
			isServer = true;
			//SERVER
			IPaddress ip; //90.253.170.185
			SDLNet_ResolveHost(&ip, NULL, PORT);

			server = SDLNet_TCP_Open(&ip);
			hs.clientIp = 11111111;
			clients.push_back(server);
		}
	}

	bool isGrowing = false;
	float movementCooldownTimer = -1;
	float previousTime = 0;
	while (!quit) {
		//system("cls");

		//TIMERS
		capTimer.start();
		//Calculate and correct fps
		float avgFPS = countedFrames / (fpsTimer.getTicks() / 1000.f);
		if (avgFPS > 2000000)
		{
			avgFPS = 0;
		}
		//cout << "Average Frames Per Second (With Cap) " << avgFPS << endl;
		++countedFrames;
		
		packet data;


		////update position
		//data.id = hs.clientIp;
		//data.dataType = BODY;
		//data.bodylength = playerBody.size();
		//for (size_t i = 0; i < playerBody.size(); i++)
		//{
		//	data.body[i] = playerBody[i];
		//}
		//cout << "Sent new Body data" << endl;
		//int ret = SDLNet_TCP_Send(server, &data, sizeof(packet));
		//cout << "Sent :" << ret << endl;

		Input();

		if (movementCooldownTimer < 0)
		{

			switch (playerDirection)
			{
				case UP:
					playerLocation.y--;
					break;

				case DOWN:
					playerLocation.y++;
					break;

				case LEFT:
					playerLocation.x--;
					break;
				case RIGHT:
					playerLocation.x++;
					break;
			}

			//coll stuff
			if (!isGrowing)
				for (size_t i = 1; i < playerBody.size(); i++)
				{
					if (playerLocation == playerBody[i])
					{
						playerLocation = vec2(rand() % gridHeight, rand() % gridHeight);
						playerBody.clear();
						playerBody.push_back(playerLocation);
						break;
					}
				}

			if (playerLocation.x < 0 || playerLocation.x > gridHeight ||
				playerLocation.y < 0 || playerLocation.y > gridWidth)
			{
				playerLocation = vec2(rand() % gridHeight, rand() % gridHeight);
				playerBody.clear();
				playerBody.push_back(playerLocation);
			}

			//bip stuff
			if (playerBody.size() != 0 && isGrowing == false)
			{
				playerBody.insert(playerBody.begin(), playerLocation);
				playerBody.pop_back();
			}

			if (playerBody[0] == bipLocation && !isGrowing)
			{
				isGrowing = true;
				playerBody.push_back(bipLocation);
				packet data;
				data.id = hs.clientIp;
				data.dataType = BIP;
				data.body[0] = vec2(-1, -1);
				int x = 5;
				int ret = SDLNet_TCP_Send(server, &data, sizeof(packet));
					cout << "Sent :" << ret << endl;

				////get new bip
				//if (int ret = SDLNet_TCP_Recv(server, &data, sizeof(packet)) != -1)
				//{
				//	cout << "Received :" << ret << endl;

				//	if (data.dataType == dataType::SERVER)
				//	{
				//		cout << "Server sent new Bip" << endl;

				//		bipLocation = data.body[0];
				//	}
				//}

			}

			isGrowing = false;
			movementCooldownTimer = 0.1;
		}

		movementCooldownTimer -= ((float)SDL_GetTicks() - previousTime) / 1000;
		
		Draw();

		previousTime = SDL_GetTicks();

		//If frame finished early
		int frameTicks = capTimer.getTicks();
		if (frameTicks < SCREEN_TICKS_PER_FRAME)
		{
			//Wait remaining time
			SDL_Delay(SCREEN_TICKS_PER_FRAME - frameTicks);
		}
	}
	SDLNet_TCP_Close(server);
	SDL_DestroyTexture(gridBackground);
	SDL_DestroyTexture(gridPlayer);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}

