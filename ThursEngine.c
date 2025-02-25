#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAP_WIDTH			16
#define MAP_HEIGHT			16
#define NUM_RAYS			120
#define TEXTURE_WIDTH		64
#define PI					3.14159265358979323846f

typedef struct {
	float		x, y;
	float		angle;
	float		fov;
	float		speed;
	float		sensitivity;
	float		sprintMultiplier;
	bool		mouseUnlocked;
} Player;

typedef struct {
	float		x, y;
	float		speed;
} Enemy;

typedef struct {
	int			width;
	int			height;
	int			data[MAP_WIDTH * MAP_HEIGHT];
} Map;

Map map = {
	.width =	MAP_WIDTH,
	.height =	MAP_HEIGHT,
	.data = {
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1,
		1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1,
		1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1,
		1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1,
		1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1,
		1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1,
		1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1,
		1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
	}
};

// Helper to fetch the map cell value (with bounds check).
int GetMapValue( Map* m, int x, int y ) {
	if( x < 0 || x >= m->width || y < 0 || y >= m->height )
		return 1;
	return m->data[y * m->width + x];
}

// Cast rays using DDA algorithm to return the distance to the wall,
// the side hit, and calculates the texture x-coordinate.
float CastRay( const Player *player, Map *m, float angle, int *side, int *texX ) {
	float		sinA = sinf( angle );
	float		cosA = cosf( angle );

	int			mapX = ( int )player->x;
	int			mapY = ( int )player->y;

	float		deltaDistX = fabsf( 1.0f / cosA );
	float		deltaDistY = fabsf( 1.0f / sinA );

	int			stepX = ( cosA < 0 ) ? -1 : 1;
	int			stepY = ( sinA < 0 ) ? -1 : 1;

	float sideDistX = ( cosA < 0 ) ? ( player->x - mapX ) * deltaDistX : ( mapX + 1.0f - player->x ) * deltaDistX;
	float sideDistY = ( sinA < 0 ) ? ( player->y - mapY ) * deltaDistY : ( mapY + 1.0f - player->y ) * deltaDistY;

	float		distance = 0.0f;
	bool		hit = false;
	while( !hit && distance < 16.0f ) {
		if( sideDistX < sideDistY ) {
			sideDistX += deltaDistX;
			mapX += stepX;
			*side = 0;
			distance = sideDistX - deltaDistX;
		} else {
			sideDistY += deltaDistY;
			mapY += stepY;
			*side = 1;
			distance = sideDistY - deltaDistY;
		}
		if( GetMapValue( m, mapX, mapY ) == 1 )
			hit = true;
	}

	// Determine exact location of where map was hit to map the texture.
	float wallHit;
	if( *side == 0 ) {
		wallHit = player->y + distance * sinA;
	} else {
		wallHit = player->x + distance * cosA;
	}
	wallHit -= floorf( wallHit );
	*texX = ( int )( wallHit * TEXTURE_WIDTH );
	if( *texX < 0 ) {
		*texX = 0;
	}
	if( *texX >= TEXTURE_WIDTH ) *texX = TEXTURE_WIDTH - 1;

	return distance;
}

// Update enemy by moving it toward player, if not too close.
void UpdateEnemy( Enemy* enemy, const Player* player, Map* m, float dt ) {
	float		dx = player->x - enemy->x;
	float		dy = player->y - enemy->y;
	float		distance = sqrtf( dx * dx + dy * dy );

	if( distance > 0.5f ) {
		float moveX = ( dx / distance ) * enemy->speed * dt;
		float moveY = ( dy / distance ) * enemy->speed * dt;

		if( GetMapValue( m, ( int )( enemy->x + moveX ), ( int )( enemy->y ) ) == 0 ) {
			enemy->x += moveX;
		}
		if( GetMapValue( m, ( int )( enemy->x ), ( int )( enemy->y + moveY ) ) == 0 ) {
			enemy->y += moveY;
		}
	}
}

// Draw Enemy to screen if it's within player's view.
void DrawEnemy( const Enemy* enemy, const Player* player ) {
	float		dx = enemy->x - player->x;
	float		dy = enemy->y - player->y;
	float		distance = sqrtf( dx * dx + dy * dy );

	if( distance > 0.2f && distance < 16.0f) {
		float enemyAngle = atan2f( dy, dx ) - player->angle;

		// Normalize angle between -PI and PI.
		if( enemyAngle < -PI ) {
			enemyAngle += 2 * PI;
		}
		if( enemyAngle > PI ) {
			enemyAngle -= 2 * PI;
		}

		if( fabsf( enemyAngle ) < player->fov / 2 ) {
			float screenX = GetScreenWidth() / 2 + tanf( enemyAngle ) * ( 500.0f / distance );
			float enemySize = fmaxf( 20.0f, fminf( 100.0f, 500.0f / distance ) );

			if( screenX + enemySize > 0 && screenX - enemySize < GetScreenWidth() ) {
				DrawRectangle( ( int )( screenX - enemySize / 2 ),
							   ( int )( GetScreenHeight() / 2 - enemySize / 2 ),
							   ( int )enemySize, ( int )enemySize,
							   ( Color ) { 255, 0, 0, 200 } );

			}
		}
	}
}

void LockMouseToCenter() {
	SetMousePosition( GetScreenWidth() / 2, GetScreenHeight() / 2 );
}

int main( void ) {
	SetConfigFlags( FLAG_WINDOW_RESIZABLE );
	InitWindow( 800, 600, "THURS" );
	SetWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS( 60 );
	HideCursor();

	printf( "Current Working Directory: %s\n", GetWorkingDirectory() );
	Texture2D wallTexture = LoadTexture("mossy.png");
	SetTextureFilter( wallTexture, TEXTURE_FILTER_POINT );

	Player player = { 4.0f, 4.0f, 0.0f, PI / 3, 2.0f, 0.002f, 1.4f };
	Enemy enemy = { 4.0f, 4.0f, 3.0f };

	

	//=======================
	//		MAIN LOOP
	//=======================
	while( !WindowShouldClose() ) {
		int screenWidth = GetScreenWidth();
		int screenHeight = GetScreenHeight();
		float dt = GetFrameTime();

		if( IsKeyPressed( KEY_TAB ) ) {
			player.mouseUnlocked = !player.mouseUnlocked;
			if( player.mouseUnlocked ) {
				EnableCursor();
			} else {
				HideCursor();
				LockMouseToCenter();
			}
		}

		if( !player.mouseUnlocked ) {
			Vector2 mouseDelta = GetMouseDelta();
			player.angle += mouseDelta.x * player.sensitivity;
			LockMouseToCenter();
		}
		float moveSpeed = player.speed * ( IsKeyDown( KEY_LEFT_SHIFT ) ? player.sprintMultiplier : 1.0f );

		//float moveSpeed = player.speed;
		if( IsKeyDown(KEY_LEFT_SHIFT)) {
			moveSpeed *= player.sprintMultiplier;
		}


		// Handle Movement using WASD (strafe uses 0.7 multiplier).
		float newX = player.x;
		float newY = player.y;


		if( IsKeyDown( KEY_W ) ) {
			newX += cosf( player.angle ) * moveSpeed * dt;
			newY += sinf( player.angle ) * moveSpeed * dt;
		}
		if( IsKeyDown( KEY_S ) ) {
			newX -= cosf( player.angle ) * moveSpeed * dt;
			newY -= sinf( player.angle ) * moveSpeed * dt;
		}
		if( IsKeyDown( KEY_A ) ) {
			newX += sinf( player.angle ) * moveSpeed * dt * 0.5f;
			newY -= cosf( player.angle ) * moveSpeed * dt * 0.5f;
		}
		if( IsKeyDown( KEY_D ) ) {
			newX -= sinf( player.angle ) * moveSpeed * dt * 0.7f;
			newY += cosf( player.angle ) * moveSpeed * dt * 0.7f;
		}
		if( IsKeyDown( KEY_LEFT ) ) {
			player.angle -= 1.5f * dt;
		}
		if( IsKeyDown( KEY_RIGHT ) ) {
			player.angle += 1.5f * dt;
		}

		// Basic collision check: only update position if the destination cell is empty.
		float collisionBuffer = 0.25f;
		if( GetMapValue( &map, ( int )( newX + collisionBuffer ), ( int )( player.y ) ) == 0 &&
			GetMapValue( &map, ( int )( newX - collisionBuffer ), ( int )( player.y ) ) == 0 ) player.x = newX;
		if( GetMapValue( &map, ( int )( player.x ), ( int )( newY + collisionBuffer ) ) == 0 &&
			GetMapValue( &map, ( int )( player.x ), ( int )( newY - collisionBuffer ) ) == 0 ) player.y = newY;

		UpdateEnemy( &enemy, &player, &map, dt );

		BeginDrawing();
		ClearBackground( BLACK );

		// Draw Floor and Ceiling.
		DrawRectangle( 0, screenHeight / 2, screenWidth, screenHeight / 2, DARKGRAY );
		DrawRectangle( 0, 0, screenWidth, screenHeight / 2, GRAY );

		float columnWidth = ( float )screenWidth / NUM_RAYS;
		for( int i = 0; i < NUM_RAYS; i++ ) {
			// Calculate ray angle for this column.
			float rayAngle = player.angle - ( player.fov / 2 ) + ( ( float )i / ( NUM_RAYS - 1 ) ) * player.fov;
			int side = 0, texX = 0;
			float distance = CastRay( &player, &map, rayAngle, &side, &texX );

			// Correct the distance to reduce fisheye distortion.
			float correctedDistance = distance * cosf( rayAngle - player.angle );
			float projectedPlane = ( screenWidth / 2 ) / tanf( player.fov / 2 );
			float wallHeight = projectedPlane / ( correctedDistance + 0.1f );

			float fog = fmaxf( 0.0f, 1.0f - ( correctedDistance / 10.0f ) );
			float brightness = ( side == 1 ) ? 0.7f : 0.8f;
			Color shade = ( Color ){ ( unsigned char )( 255 * fog * brightness ),
									 ( unsigned char )( 255 * fog * brightness ),
									 ( unsigned char )( 255 * fog * brightness ), 255 };

			Rectangle srcRect = { ( float )texX, 0, 1, ( float )wallTexture.height };
			Rectangle destRect = { i * columnWidth, ( screenHeight - wallHeight ) / 2, columnWidth, wallHeight };
			DrawTexturePro( wallTexture, srcRect, destRect, ( Vector2 ) { 0, 0 }, 0.0f, shade );
		}
		DrawEnemy( &enemy, &player );

		DrawFPS( 10, 10 );
		EndDrawing();
	}

	UnloadTexture( wallTexture );
	CloseWindow();

	return 0;
}