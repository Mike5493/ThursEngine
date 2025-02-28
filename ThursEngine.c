#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAP_WIDTH		16
#define MAP_HEIGHT		16
#define NUM_RAYS		120
#define TEXTURE_WIDTH	64
#define PI	3.14159265358979323846f

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
	float		doorTimers[MAP_WIDTH * MAP_HEIGHT];
	int			doorOriginalX[MAP_WIDTH * MAP_HEIGHT];
	int			doorOriginalY[MAP_WIDTH * MAP_HEIGHT];
} Map;

Map map = {
	.width = MAP_WIDTH,
	.height = MAP_HEIGHT,
	.data = {
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1,
		1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1,
		1, 0, 1, 0, 1, 1, 2, 1, 1, 1, 2, 1, 0, 0, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1,
		1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 1,
		1, 0, 0, 0, 0, 1, 2, 1, 1, 1, 1, 0, 1, 0, 0, 1,
		1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1,
		1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1,
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
float CastRay( const Player* player, Map* m, float angle, int* side, int* texX, int* hitType ) {
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

	*hitType = 0; // Default: No special hit

	Color shade = ( Color ){ 255, 255, 255, 255 };

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

		int tile = GetMapValue( m, mapX, mapY );
		if( tile == 1 || tile == 2 ) {
			hit = true;
			if( tile == 2 ) {
				*hitType = 2; // Door hit
			}
		}
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

	if( distance > 0.2f && distance < 16.0f ) {
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
							   ( Color ) {
					255, 0, 0, 200
				} );
			}
		}
	}
}

void LockMouseToCenter() {
	SetMousePosition( GetScreenWidth() / 2, GetScreenHeight() / 2 );
}

// Door collision helper.
bool isPassable( int x, int y ) {
	int tile = GetMapValue( &map, x, y );
	return( tile == 0 || tile == 2 );
}

void ToggleDoor( Player* player, Map* m ) {
	int px = ( int )player->x;
	int py = ( int )player->y;

	int nx = ( int )( px + cosf( player->angle ) + 0.5f );
	int ny = ( int )( py + sinf( player->angle ) + 0.5f );

	if( nx >= 0 && nx < m->width && ny >= 0 && ny < m->height ) {
		int index = ny * m->width + nx;

		// If it's a door, move it left or right
		if( m->data[index] == 2 ) {
			int moveX = ( cosf( player->angle ) > 0 ) ? 1 : -1;
			int targetX = nx + moveX;

			if( targetX >= 0 && targetX < m->width && GetMapValue( m, targetX, ny ) == 0 ) {
				// Move door
				m->data[index] = 0;
				m->data[ny * m->width + targetX] = 2;
				m->doorTimers[ny * m->width + targetX] = 3.0f; // Stay open for 3 seconds
				m->doorOriginalX[ny * m->width + targetX] = nx; // Store original position
				m->doorOriginalY[ny * m->width + targetX] = ny;
			}
		}
	}
}

void UpdateDoors( Map* m, float dt ) {
	for( int y = 0; y < MAP_HEIGHT; y++ ) {
		for( int x = 0; x < MAP_WIDTH; x++ ) {
			int index = y * MAP_WIDTH + x;
			if( m->doorTimers[index] > 0 ) {
				m->doorTimers[index] -= dt;
				if( map.doorTimers[index] <= 0 ) {
					int originalX = map.doorOriginalX[index];
					int originalY = map.doorOriginalY[index];

					if( originalX >= 0 && originalY >= 0 && map.data[index] == 2 ) {
						m->data[originalY * MAP_WIDTH + originalX] = 2;  // Move door back
						m->data[index] = 0; // Clear moved position
						m->doorTimers[index] = 0; // Reset timer
					}
				}
			}
		}
	}
}


int main( void ) {
	SetConfigFlags( FLAG_WINDOW_RESIZABLE );
	InitWindow( 800, 600, "THURS" );
	SetWindowState( FLAG_WINDOW_RESIZABLE );
	SetTargetFPS( 60 );
	HideCursor();

	printf( "Current Working Directory: %s\n", GetWorkingDirectory() );
	Texture2D wallTexture = LoadTexture( "mossy.png" );
	SetTextureFilter( wallTexture, TEXTURE_FILTER_POINT );

	Player player = { 7.0f, 7.0f, 0.0f, PI / 3, 2.0f, 0.002f, 1.4f };
	Enemy enemy = { 3.0f, 3.0f, 3.0f };



	//=======================
	// MAIN LOOP
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

		float moveSpeed = player.speed * ( IsKeyDown( KEY_LEFT_SHIFT ) ? player.sprintMultiplier : 1.0f );

		//float moveSpeed = player.speed;
		if( IsKeyDown( KEY_LEFT_SHIFT ) ) {
			moveSpeed *= player.sprintMultiplier;
		}

		if( !player.mouseUnlocked ) {
			Vector2 mouseDelta = GetMouseDelta();
			player.angle += mouseDelta.x * player.sensitivity;
			LockMouseToCenter();
		}
		if( IsKeyPressed( KEY_E ) ) {
			ToggleDoor( &player, &map );
		}
		UpdateDoors( &map, dt );

		// Handle Movement using WASD (strafe uses 0.7 multiplier).
		float newX = player.x;
		float newY = player.y;

		float moveForward = IsKeyDown( KEY_W ) * moveSpeed * dt;
		float moveBackward = IsKeyDown( KEY_S ) * moveSpeed * dt;
		float strafeLeft = IsKeyDown( KEY_A ) * moveSpeed * dt;
		float strafeRight = IsKeyDown( KEY_D ) * moveSpeed * dt;

		float turnLeft = IsKeyDown( KEY_LEFT ) * ( 1.5f * dt );
		float turnRight = IsKeyDown( KEY_RIGHT ) * ( 1.5f * dt );

		newX += cosf( player.angle ) * ( moveForward - moveBackward ) + sinf( player.angle ) * ( strafeLeft - strafeRight );
		newY += sinf( player.angle ) * ( moveForward - moveBackward ) - cosf( player.angle ) * ( strafeLeft - strafeRight );
		player.angle += turnRight - turnLeft;


		// Basic collision check: only update position if the destination cell is empty.
		float collisionBuffer = 0.1f;

		if( GetMapValue( &map, ( int )( newX + collisionBuffer ), ( int )( player.y ) ) == 0 &&
			GetMapValue( &map, ( int )( newX - collisionBuffer ), ( int )( player.y ) ) == 0 ) {
			player.x = newX;
		}

		else if( GetMapValue( &map, ( int )( player.x ), ( int )( newY + collisionBuffer ) ) == 0 &&
				 GetMapValue( &map, ( int )( player.x ), ( int )( newY - collisionBuffer ) ) == 0 ) {
			player.y = newY;
		}

		// Check for vertical movement (Y-Axis)
		if( GetMapValue( &map, ( int )( player.x ), ( int )( newY + collisionBuffer ) ) == 0 &&
			GetMapValue( &map, ( int )( player.x ), ( int )( newY - collisionBuffer ) ) == 0 ) {
			player.y = newY;
		}
		// Allow sliding on X-Axis if Y movement is blocked
		else if( GetMapValue( &map, ( int )( newX + collisionBuffer ), ( int )( player.y ) ) == 0 &&
				 GetMapValue( &map, ( int )( newX - collisionBuffer ), ( int )( player.y ) ) == 0 ) {
			player.x = newX;
		}

		// Check X movement
		if( isPassable( ( int )( newX + collisionBuffer ), ( int )( player.y ) ) &&
			isPassable( ( int )( newX - collisionBuffer ), ( int )( player.y ) ) ) {
			player.x = newX;
		}

		// Check Y movement
		if( isPassable( ( int )( player.x ), ( int )( newY + collisionBuffer ) ) &&
			isPassable( ( int )( player.x ), ( int )( newY - collisionBuffer ) ) ) {
			player.y = newY;
		}

		UpdateEnemy( &enemy, &player, &map, dt );

		BeginDrawing();
		ClearBackground( BLACK );

		// Draw Floor and Ceiling.
		DrawRectangle( 0, screenHeight / 2, screenWidth, screenHeight / 2, DARKGRAY );
		DrawRectangle( 0, 0, screenWidth, screenHeight / 2, BLACK );

		float columnWidth = ( float )screenWidth / NUM_RAYS;
		for( int i = 0; i < NUM_RAYS; i++ ) {
			// Calculate ray angle for this column.
			float rayAngle = player.angle - ( player.fov / 2 ) + ( ( float )i / ( NUM_RAYS - 1 ) ) * player.fov;
			int side = 0, texX = 0, hitType = 0;
			float distance = CastRay( &player, &map, rayAngle, &side, &texX, &hitType );

			// Correct the distance to reduce fisheye distortion.
			float correctedDistance = distance * cosf( rayAngle - player.angle );
			float projectedPlane = ( screenWidth / 2 ) / tanf( player.fov / 2 );
			float wallHeight = projectedPlane / ( correctedDistance + 0.1f );

			float fog = fmaxf( 0.0f, 1.0f - ( correctedDistance / 10.0f ) );
			float brightness = ( side == 1 ) ? 0.7f : 0.8f;
			Color shade = ( Color ){ ( unsigned char )( 255 * fog * brightness ),
									 ( unsigned char )( 255 * fog * brightness ),
									 ( unsigned char )( 255 * fog * brightness ), 255 };
			if( hitType == 2 ) {
				shade = ( Color ){ 150, 75, 0, 255 };
			}


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