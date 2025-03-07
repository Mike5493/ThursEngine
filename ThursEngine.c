/*
*==========================================================================
*                      **THURSENGINE**                                    *
***************************************************************************
* This is a raycasting engine made in pure c99, with Raylib for rendering *
* and input.                                                              *
* Thank you so much to Ben for the fix to the doors!                      *
*                                                                         *
* Author: Mikey                                                           *
* Date: 02/24/2025                                                        *
*                                                                         *
*==========================================================================
*/

#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAP_WIDTH		 64
#define MAP_HEIGHT		 64
#define NUM_RAYS		 640

#define TEXTURE_WIDTH	 64
#define SCREEN_H		800
#define SCREEN_W		600

#define NUM_ENTITIES	  6

#define PI	3.14159265358979323846f
#define CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

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
	Color		color;
	int			behavior;		// 0 = chase, 1 = wander, 2 = stationary
} Entity;
Entity entities[NUM_ENTITIES];

typedef enum { CLOSED, OPENING, OPEN, CLOSING } DoorState;
typedef struct {
	int			width;
	int			height;
	int			data[MAP_WIDTH * MAP_HEIGHT];
	float		doorTimers[MAP_WIDTH * MAP_HEIGHT];
	int			doorOriginalX[MAP_WIDTH * MAP_HEIGHT];
	int			doorOriginalY[MAP_WIDTH * MAP_HEIGHT];
	float		doorOpenness[MAP_WIDTH * MAP_HEIGHT];
	DoorState	doorStates[MAP_WIDTH * MAP_HEIGHT];
} Map;

Map map = {
	.width = MAP_WIDTH,
	.height = MAP_HEIGHT,
	.data = {0}
};

void GenerateMap( Map* m ) {
	for( int y = 0; y < MAP_HEIGHT; y++ ) {
		for( int x = 0; x < MAP_WIDTH; x++ ) {
			if( y == 0 || y == MAP_HEIGHT - 1 || x == 0 || x == MAP_WIDTH - 1 ) {
				m->data[y * MAP_WIDTH + x] = 1; // Solid walls at borders
			} else if( rand() % 100 < 10 ) { // 10% chance of placing walls randomly
				m->data[y * MAP_WIDTH + x] = 1;
			} else {
				m->data[y * MAP_WIDTH + x] = 0; // Open space
			}
		}
	}
}

// Door collision helper.
bool isPassable( int x, int y ) {
	int tile = GetMapValue( &map, x, y );
	if( tile == 2 ) {
		int index = y * map.width + x;
		return map.doorOpenness[index] > 0.5f; // Passable when more than half open
	}
	return tile == 0;
}

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

	float		sideDistX = ( cosA < 0 ) ? ( player->x - mapX ) * deltaDistX : ( mapX + 1.0f - player->x ) * deltaDistX;
	float		sideDistY = ( sinA < 0 ) ? ( player->y - mapY ) * deltaDistY : ( mapY + 1.0f - player->y ) * deltaDistY;

	float		distance = 0.0f;
	bool		hit = false;

	*hitType = 0; // Default: No special hit

	//Color shade = ( Color ){ 255, 255, 255, 255 };

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
		if( tile == 1 || ( tile == 2 && m->doorOpenness[mapY * m->width + mapX] < 0.5f ) ) {
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
	*texX = ( int )roundf( wallHit * TEXTURE_WIDTH );

	if( *texX < 0 ) {
		*texX = 0;
	}
	if( *texX >= TEXTURE_WIDTH ) *texX = TEXTURE_WIDTH - 1;

	return distance;
}

// Update enemy by moving it toward player, if not too close.
void UpdateEntities( Entity* entities, int count, const Player* player, Map* m, float dt ) {
	for( int i = 0; i < count; i++ ) {
		float newX = entities[i].x;
		float newY = entities[i].y;

		switch( entities[i].behavior ) {
			case 0: // Chase player
			{
				float dx = player->x - entities[i].x;
				float dy = player->y - entities[i].y;
				float distance = sqrtf( dx * dx + dy * dy );
				if( distance > 0.5f ) {
					newX += ( dx / distance ) * entities[i].speed * dt;
					newY += ( dy / distance ) * entities[i].speed * dt;
				}
			}
			break;
			case 1: // Wander randomly
			{
				static float wanderTimer = 0.0f;
				wanderTimer += dt;
				if( wanderTimer > 1.0f ) {
					newX += ( ( float )rand() / RAND_MAX - 0.5f ) * entities[i].speed * dt * 2.0f;
					newY += ( ( float )rand() / RAND_MAX - 0.5f ) * entities[i].speed * dt * 2.0f;
					wanderTimer = 0.0f;
				}
			}
			break;
			case 2: // Stationary
				break;
		}

		// Collision check
		int newCellX = ( int )newX;
		int newCellY = ( int )newY;
		if( isPassable( newCellX, newCellY ) ) {
			entities[i].x = newX;
			entities[i].y = newY;
		} else {
			// Simple slide (keep original position if blocked)
			entities[i].x = entities[i].x;
			entities[i].y = entities[i].y;
		}
	}
}

// Draw Enemy to screen if it's within player's view.
void DrawEntities( Entity* entities, int count, const Player* player, Map* m ) {
	for( int i = 0; i < count; i++ ) {
		float dx = entities[i].x - player->x;
		float dy = entities[i].y - player->y;
		float distance = sqrtf( dx * dx + dy * dy );

		if( distance > 0.2f && distance < 16.0f ) {
			float entityAngle = atan2f( dy, dx ) - player->angle;
			if( entityAngle < -PI ) entityAngle += 2 * PI;
			if( entityAngle > PI ) entityAngle -= 2 * PI;

			if( fabsf( entityAngle ) < player->fov / 2 ) {
				// Use CastRay to check occlusion
				int side, texX, hitType;
				float occlusionDistance = CastRay( player, m, entityAngle + player->angle, &side, &texX, &hitType );

				// If the ray hits a wall before the entity, it's occluded
				bool occluded = ( occlusionDistance <= distance && occlusionDistance < 16.0f );

				if( !occluded ) {
					float screenX = GetScreenWidth() / 2 + tanf( entityAngle ) * ( 500.0f / distance );
					int entitySize = ( int )fmaxf( 20.0f, fminf( 100.0f, 500.0f / distance ) ); // Revert to truncation for now

					if( screenX + entitySize > 0 && screenX - entitySize < GetScreenWidth() ) {
						DrawRectangle( ( int )( screenX - entitySize / 2 ), // Revert to truncation
									   ( int )( GetScreenHeight() / 2 - entitySize / 2 ),
									   entitySize, entitySize,
									   entities[i].color );
						// Debug: Confirm drawing
						//printf("Drawing entity %d at %.1f,%.1f, distance: %.1f, occluded: %d\n", i, entities[i].x, entities[i].y, distance, occluded);
					}
				}
			}
		}
	}
}

void LockMouseToCenter() {
	SetMousePosition( GetScreenWidth() / 2, GetScreenHeight() / 2 );
}


void ToggleDoor( Player* player, Map* m ) {
	int px = ( int )player->x;
	int py = ( int )player->y;

	//printf( "Player at %d,%d trying to toggle door\n", px, py );

	int minDist = MAP_WIDTH + MAP_HEIGHT; // Large initial distance
	int targetX = -1, targetY = -1;

	// Check 3x3 area around player
	for( int dy = -1; dy <= 1; dy++ ) {
		for( int dx = -1; dx <= 1; dx++ ) {
			int nx = px + dx;
			int ny = py + dy;
			if( nx >= 0 && nx < m->width && ny >= 0 && ny < m->height ) {
				int index = ny * m->width + nx;
				if( m->data[index] == 2 ) {
					int dist = abs( dx ) + abs( dy ); // Manhattan distance
					if( dist < minDist ) {
						minDist = dist;
						targetX = nx;
						targetY = ny;
					}
				}
			}
		}
	}

	if( targetX != -1 && targetY != -1 ) {
		int index = targetY * m->width + targetX;
		if( m->doorStates[index] == CLOSED || m->doorStates[index] == CLOSING ) {
			m->doorTimers[index] = 3.0f; // Total cycle: 1s open, 1s wait, 1s close
			m->doorStates[index] = OPENING;
			//printf( "Door toggled open at %d,%d, timer set to %f\n", targetX, targetY, m->doorTimers[index] );
		}
	}
}


void UpdateDoors( Map* m, float dt ) {
	for( int y = 0; y < MAP_HEIGHT; y++ ) {
		for( int x = 0; x < MAP_WIDTH; x++ ) {
			int index = y * MAP_WIDTH + x;
			if( m->data[index] == 2 && m->doorTimers[index] > 0 ) { // Only check doors with active timers
				m->doorTimers[index] -= dt;
				if( m->doorTimers[index] < 0.0f ) m->doorTimers[index] = 0.0f; // Prevent negative timer

				switch( m->doorStates[index] ) {
					case OPENING:
						if( m->doorTimers[index] > 2.0f ) {
							m->doorOpenness[index] += dt / 1.0f;
							if( m->doorOpenness[index] > 1.0f ) m->doorOpenness[index] = 1.0f;
						} else {
							m->doorStates[index] = OPEN;
						}
						break;
					case OPEN:
						if( m->doorTimers[index] <= 1.0f ) {
							m->doorStates[index] = CLOSING;
						}
						break;
					case CLOSING:
						if( m->doorOpenness[index] > 0.0f ) {
							m->doorOpenness[index] -= dt / 1.0f;
							if( m->doorOpenness[index] < 0.0f ) m->doorOpenness[index] = 0.0f;
						} else {
							m->doorStates[index] = CLOSED;
							m->data[index] = 2;
							m->doorTimers[index] = 0.0f;
							//printf( "Door %d,%d closed\n", x, y ); // Debug only on close
						}
						break;
					case CLOSED:
						break;
				}
				//printf( "Door %d,%d openness: %f, timer: %f, state: %d\n", x, y, m->doorOpenness[index], m->doorTimers[index], m->doorStates[index] ); // Debug only active doors
			}
		}
	}
}

// Dithering Formula for textured floor using bit shifting.
unsigned int hash( unsigned int x, unsigned int y ) {
	x = ( x ^ 61 ) ^ ( y >> 16 );
	x = x + ( x << 3 );
	x = x ^ ( x >> 4 );
	x = x * 0x27d4eb2d;
	x = x ^ ( x >> 15 );
	return x;
}

int main( void ) {
	SetConfigFlags( FLAG_WINDOW_RESIZABLE );
	InitWindow( 800, 600, "THURS" );
	SetWindowState( FLAG_WINDOW_RESIZABLE );
	SetTargetFPS( 60 );
	HideCursor();

	printf( "Current Working Directory: %s\n", GetWorkingDirectory() );
	Texture2D wallTexture = LoadTexture( "mossy.png" );
	//Texture2D hudTexture = LoadTexture( "hud.png" );
	//Texture2D floorTexture = LoadTexture( "floor.png" );
	//Texture2D ceilingTexture = LoadTexture( "ceiling.png" );
	SetTextureFilter( wallTexture, TEXTURE_FILTER_POINT );

	GenerateMap( &map );

	Player player = { 10.0f, 10.0f, 0.0f, PI / 3, 2.0f, 0.002f, 1.4f };
	while( !isPassable( ( int )player.x, ( int )player.y ) ) {
		player.x += 0.1f;
		if( player.x >= MAP_WIDTH ) {
			player.x = 0.1f;
			player.y = 0.1f;
		}
		if( player.y >= MAP_HEIGHT ) break;
	}

	//==============================
   // 5 is max Y for monster closet
   //===============================
	entities[0] = ( Entity ){ 2.0f, 2.0f, 1.0f, ( Color ) { 255, 0, 0, 200 }, 0 };     // Red, fast chase
	entities[1] = ( Entity ){ 2.0f, 4.0f, 0.5f, ( Color ) { 0, 255, 0, 200 }, 1 };   // Green, slow wander
	entities[2] = ( Entity ){ 4.0f, 2.0f, 0.0f, ( Color ) { 0, 0, 255, 200 }, 2 };   // Blue, stationary
	entities[3] = ( Entity ){ 2.0f, 6.0f, 1.5f, ( Color ) { 255, 255, 0, 200 }, 0 }; // Yellow, very fast chase
	entities[4] = ( Entity ){ 6.0f, 2.0f, 0.7f, ( Color ) { 0, 255, 255, 200 }, 1 }; // Cyan, moderate wander
	entities[5] = ( Entity ){ 4.0f, 4.0f, 0.0f, ( Color ) { 255, 0, 255, 200 }, 2 }; // Magenta, stationary	// Magenta, stationary

	RenderTexture2D target = LoadRenderTexture( 800, 600 );

	for( int y = 0; y < MAP_HEIGHT; y++ ) {
		for( int x = 0; x < MAP_WIDTH; x++ ) {
			int index = y * MAP_WIDTH + x;
			map.doorOpenness[index] = 0.0f;
			map.doorStates[index] = CLOSED;
		}
	}

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


		// Collision check with larger buffer and diagonal collision check
		float collisionBuffer = 0.1;
		int			newCellX = ( int )roundf( newX );
		int			newCellY = ( int )roundf( newY );
		int			currCellX = ( int )roundf( player.x );
		int			currCellY = ( int )roundf( player.y );

		// Original X-axis check with diagonal corners
		bool xPassable = isPassable( ( int )( newX + collisionBuffer ), ( int )player.y ) &&
			isPassable( ( int )( newX - collisionBuffer ), ( int )player.y ) &&
			isPassable( ( int )( newX + collisionBuffer ), ( int )( player.y + collisionBuffer ) ) && // Top-right
			isPassable( ( int )( newX + collisionBuffer ), ( int )( player.y - collisionBuffer ) ) && // Bottom-right
			isPassable( ( int )( newX - collisionBuffer ), ( int )( player.y + collisionBuffer ) ) && // Top-left
			isPassable( ( int )( newX - collisionBuffer ), ( int )( player.y - collisionBuffer ) );   // Bottom-left

		// Original Y-axis check with diagonal corners
		bool yPassable = isPassable( ( int )player.x, ( int )( newY + collisionBuffer ) ) &&
			isPassable( ( int )player.x, ( int )( newY - collisionBuffer ) ) &&
			isPassable( ( int )( player.x + collisionBuffer ), ( int )( newY + collisionBuffer ) ) && // Top-right
			isPassable( ( int )( player.x - collisionBuffer ), ( int )( newY + collisionBuffer ) ) && // Top-left
			isPassable( ( int )( player.x + collisionBuffer ), ( int )( newY - collisionBuffer ) ) && // Bottom-right
			isPassable( ( int )( player.x - collisionBuffer ), ( int )( newY - collisionBuffer ) );   // Bottom-left

		if( xPassable ) player.x = newX;
		if( yPassable ) player.y = newY;

		// Fallback sliding logic with diagonal awareness
		if( !xPassable && !yPassable ) {
			// Try sliding along X if Y is blocked
			if( isPassable( newCellX, currCellY ) &&
				isPassable( newCellX + collisionBuffer, currCellY ) &&
				isPassable( newCellX - collisionBuffer, currCellY ) &&
				isPassable( newCellX + collisionBuffer, currCellY + collisionBuffer ) &&
				isPassable( newCellX + collisionBuffer, currCellY - collisionBuffer ) &&
				isPassable( newCellX - collisionBuffer, currCellY + collisionBuffer ) &&
				isPassable( newCellX - collisionBuffer, currCellY - collisionBuffer ) ) {
				player.x = newX;
			}
			// Try sliding along Y if X is blocked
			else if( isPassable( currCellX, newCellY ) &&
					 isPassable( currCellX, newCellY + collisionBuffer ) &&
					 isPassable( currCellX, newCellY - collisionBuffer ) &&
					 isPassable( currCellX + collisionBuffer, newCellY + collisionBuffer ) &&
					 isPassable( currCellX - collisionBuffer, newCellY + collisionBuffer ) &&
					 isPassable( currCellX + collisionBuffer, newCellY - collisionBuffer ) &&
					 isPassable( currCellX - collisionBuffer, newCellY - collisionBuffer ) ) {
				player.y = newY;
			}
		}


		UpdateDoors( &map, dt );
		UpdateEntities( entities, NUM_ENTITIES, &player, &map, dt );

		BeginTextureMode( target );
		ClearBackground( BLACK );

		for( int i = 0; i < screenHeight / 2; i += 4 ) {
			int shade = 20 + ( i / 4 ) * 2;
			DrawRectangle( 0, i, screenWidth, 4, ( Color ) { shade, shade, shade, 100 } );  // Ceiling
		}

		for( int i = screenHeight / 2; i < screenHeight; i += 4 ) {
			int baseShade = 90 + ( ( i - screenHeight / 2 ) / 4 ) * 3;
			for( int j = 0; j < 4; j++ ) {
				int yOffset = i + j;
				if( yOffset >= screenHeight ) break;
				// Dithering procedure
				int blockX = ( yOffset * screenWidth ) / 4;
				int blockY = yOffset / 4;
				int noise = ( hash( ( unsigned int )( blockX + blockY ), 0 ) % 10 ) - 5;
				int shade = baseShade + noise;
				Color floorColor = {
					( unsigned char )CLAMP( shade - 10, 0, 255 ),
					( unsigned char )CLAMP( shade - 15, 0, 255 ),
					( unsigned char )CLAMP( shade - 20, 0, 255 ),
					80
				};
				DrawRectangle( 0, yOffset, screenWidth, 1, floorColor );
			}
		}


		float columnWidth = ( float )800 / NUM_RAYS;
		for( int i = 0; i < NUM_RAYS; i++ ) {
			// Calculate ray angle for this column.
			float rayAngle = player.angle - ( player.fov / 2 ) + ( ( float )i / ( NUM_RAYS - 1 ) ) * player.fov;
			int side = 0, texX = 0, hitType = 0;
			float distance = CastRay( &player, &map, rayAngle, &side, &texX, &hitType );

			// Correct the distance to reduce fisheye distortion.
			float correctedDistance = distance * cosf( rayAngle - player.angle );
			float projectedPlane = ( 800 / 2 ) / tanf( player.fov / 2 );
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
			Rectangle destRect = { i * columnWidth, ( 600 - wallHeight ) / 2, columnWidth, wallHeight };
			DrawTexturePro( wallTexture, srcRect, destRect, ( Vector2 ) { 0, 0 }, 0.0f, shade );

		}
		DrawEntities( entities, NUM_ENTITIES, &player, &map );
		DrawFPS( 10, 10 );
		EndTextureMode();

		BeginDrawing();
		ClearBackground( BLACK );


		float scale = fminf( ( float )screenWidth / 800, ( float )screenHeight / 600 );
		float offsetX = ( screenWidth - 800 * scale ) / 2;
		float offsetY = ( screenHeight - 600 * scale ) / 2;

		DrawTexturePro( target.texture,
						( Rectangle ) {
			0, 0, ( float )target.texture.width, ( float )-target.texture.height
		},
						( Rectangle ) {
			offsetX, offsetY, 800 * scale, 600 * scale
		},  // Proper scaling
						( Vector2 ) {
			0, 0
		}, 0.0f, WHITE );



		// Draw Border textures
		//if( offsetX > 0 ) {
		//	 // Draw on the left black bar
  //          DrawTexturePro( hudTexture,
  //                          ( Rectangle ){ 0, 0, ( float )hudTexture.width, ( float )hudTexture.height },
  //                          ( Rectangle ){ 0, 0, offsetX, ( float )screenHeight },
  //                          ( Vector2 ){ 0, 0 }, 0.0f, WHITE );
  //          // Draw on the right black bar
  //          DrawTexturePro( hudTexture,
  //                          ( Rectangle ){ 0, 0, ( float )hudTexture.width, ( float )hudTexture.height },
  //                          ( Rectangle ){ screenWidth - offsetX, 0, offsetX, ( float )screenHeight },
  //                          ( Vector2 ){ 0, 0 }, 0.0f, WHITE );
  //      }

		EndDrawing();
	}

	UnloadTexture( wallTexture );
	//UnloadTexture( hudTexture );
	//UnloadTexture( floorTexture );
	//UnloadTexture( ceilingTexture );
	UnloadRenderTexture( target );
	CloseWindow();

	return 0;
}