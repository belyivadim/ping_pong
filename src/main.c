#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "raylib.h"
#include "raymath.h"

#include "network.h"

#ifndef WINDOW_SIDE
#define WINDOW_SIDE 200
#endif /* !WINDOW_SIDE */

#define WINDOW_WIDTH_RATIO 4
#define WINDOW_HEIGHT_RATIO 3

#define WINDOW_WIDTH (int)(WINDOW_SIDE * WINDOW_WIDTH_RATIO)
#define WINDOW_HEIGHT (int)(WINDOW_SIDE * WINDOW_HEIGHT_RATIO)

#define PADDLE_HEIGHT (int)(WINDOW_SIDE / 2.67f)
#define PADDLE_WIDTH (int)(WINDOW_SIDE / 13.33)
#define MAX_PADDLE_SPEED (int)(WINDOW_SIDE / 0.29f)
#define MIN_PADDLE_SPEED (int)(WINDOW_SIDE / 0.57f)
#define PADDLE_SPEED (int)(WINDOW_SIDE / 0.5f)

#define BALL_SPEED (int)(WINDOW_SIDE / 0.44f)
#define MAX_BALL_SPEED (int)(WINDOW_SIDE / 0.22f)
#define MIN_BALL_SPEED BALL_SPEED
#define BALL_SIDES (int)(WINDOW_SIDE / 13.33f)

#define TAIL_CAPACITY_BALL 15
#define TAIL_CAPACITY_PADDLE 24
#define TAIL_STRUCT(size) Vector2 tail[(size)]; int tail_begin; int tail_len

#define BACKGROUND_COLOR CLITERAL(Color){ 30, 20, 40, 255 }// CLITERAL(Color){ 0, 37, 14, 255 }
#define MAIN_UI_COLOR PURPLE

typedef struct {
  Rectangle rect;
  Color color;
  float speed;
  Vector2 direction;
  TAIL_STRUCT(TAIL_CAPACITY_PADDLE);
} Paddle;

typedef struct {
  Rectangle rect;
  Color color;
  float speed;
  float spin_factor;
  Vector2 direction;
  TAIL_STRUCT(TAIL_CAPACITY_BALL);
} Ball;

typedef struct GameContext GameContext;
typedef void (*UpdateFn)(GameContext *ctx, float dt);

struct GameContext {
  Paddle paddles[2];
  Ball ball;
  int scores[2];
  UpdateFn update;
  UdpSocket server_sock;
  UdpSocket client_sock;
  int pressed_key[2];
  bool is_paused;
};

typedef enum {
  GAME_LOCAL,
  GAME_NETWORK_HOST,
  GAME_NETWORK_CLIENT,
} GameKind;

typedef struct {
  char *prog;
  GameKind game_kind;
  const char *host_addr;
  int host_port;
} CmdConfig;


static void game_local_update(GameContext *ctx, float dt);
static void game_client_update(GameContext *ctx, float dt);
static void game_host_pending_update(GameContext *ctx, float dt);
static void game_host_update(GameContext *ctx, float dt);


static void clamp_rect_within_screen(Rectangle *p_rect) {
  p_rect->y = Clamp(p_rect->y, 0, WINDOW_HEIGHT - p_rect->height);
  p_rect->x = Clamp(p_rect->x, 0, WINDOW_WIDTH - p_rect->width);
}

static void handle_collision(Ball *p_ball, Paddle *p_paddle) {
  p_ball->color = p_paddle->color;
  p_ball->direction.x = -p_ball->direction.x;
  p_ball->direction.y = -p_ball->direction.y;

  float collision_point = (p_ball->rect.y + p_ball->rect.height / 2) - (p_paddle->rect.y + p_paddle->rect.height / 2);
  float ball_speed_factor = 1.0f;
  float reflection_angle = 0.f;

  bool are_opposite_Y_directions = (p_ball->direction.y * p_paddle->direction.y) < 0.f
    || (p_ball->direction.y == 0 && p_paddle->direction.y != 0)
    || (p_ball->direction.y != 0 && p_paddle->direction.y == 0);
  float speed_diff = fabsf(p_ball->speed - p_paddle->speed);
  p_ball->spin_factor = 0.020f * (powf(speed_diff, 0.5f) + 0.5f) * are_opposite_Y_directions;
  p_ball->direction = Vector2Rotate(p_ball->direction, -p_ball->spin_factor);

  float collision_point_abs = fabsf(collision_point);
  if (collision_point_abs <= p_paddle->rect.height * 0.35f) {
    ball_speed_factor += fabsf(collision_point / (p_paddle->rect.height * 0.25f)) * 0.5f;
  } else if (collision_point_abs > p_paddle->rect.height * 0.45f) {
    ball_speed_factor -= fabsf(collision_point / (p_paddle->rect.height * 0.75f)) * 0.5f;
  }

  if (collision_point_abs <= p_paddle->rect.height * 0.35f) {
    reflection_angle = collision_point / (p_paddle->rect.height * 0.25f) * 0.2f * !!p_paddle->direction.y;
  } else {
    reflection_angle = collision_point / (p_paddle->rect.height * 0.75f) * 0.2f * !!p_paddle->direction.y;
  }

  if (0 == collision_point_abs || collision_point_abs <= p_paddle->rect.height * 0.15f) {
    p_paddle->speed = Clamp(p_paddle->speed + 50, MIN_PADDLE_SPEED, MAX_PADDLE_SPEED);
  } else if (collision_point_abs > p_paddle->rect.height * 0.25f) {
    p_paddle->speed = Clamp(p_paddle->speed - 50, MIN_PADDLE_SPEED, MAX_PADDLE_SPEED);
  }


  p_ball->speed = Clamp(p_ball->speed * ball_speed_factor, MIN_BALL_SPEED, MAX_BALL_SPEED);
  p_ball->direction = Vector2Rotate(p_ball->direction, reflection_angle);
}


static void handle_pressed_key(GameContext *ctx, int key_index) {
  switch (ctx->pressed_key[key_index]) {
    case 0: ctx->paddles[key_index].direction.y = 0; break;
    case KEY_DOWN: ctx->paddles[key_index].direction.y = 1; break;
    case KEY_UP: ctx->paddles[key_index].direction.y = -1; break;
  }
}

static void handle_input(GameContext *ctx, float dt) {
  (void)dt;

  if (IsKeyReleased(KEY_DOWN) || IsKeyReleased(KEY_UP)) {
    ctx->pressed_key[1] = 0;
    ctx->paddles[1].direction.y = 0;
  }

  if (IsKeyReleased(KEY_W) || IsKeyReleased(KEY_S)) {
    ctx->pressed_key[0] = 0;
    ctx->paddles[0].direction.y = 0;
  }

  if (IsKeyDown(KEY_S)) {
    ctx->pressed_key[0] = KEY_DOWN;
    ctx->paddles[0].direction.y = 1;
  }

  if (IsKeyDown(KEY_W)) {
    ctx->pressed_key[0] = KEY_UP;
    ctx->paddles[0].direction.y = -1;
  }

  if (IsKeyDown(KEY_DOWN)) {
    ctx->pressed_key[1] = KEY_DOWN;
    ctx->paddles[1].direction.y = 1;
  }

  if (IsKeyDown(KEY_UP)) {
    ctx->pressed_key[1] = KEY_UP;
    ctx->paddles[1].direction.y = -1;
  }
}

static GameContext game_init(const CmdConfig *p_cfg) {

  Paddle p1 = {
    .rect = { 
      .x = 30, 
      .y = (float)WINDOW_HEIGHT / 2 - (float)PADDLE_HEIGHT / 2, 
      .width = PADDLE_WIDTH, 
      .height = PADDLE_HEIGHT 
    },
    .color = SKYBLUE,
    .speed = PADDLE_SPEED,
    .direction = {0}
  };

  Paddle p2 = {
    .rect = { 
      .x = WINDOW_WIDTH - 30 - PADDLE_WIDTH, 
      .y = (float)WINDOW_HEIGHT / 2 - (float)PADDLE_HEIGHT / 2, 
      .width = PADDLE_WIDTH, 
      .height = PADDLE_HEIGHT 
    },
    .color = MAGENTA,
    .speed = PADDLE_SPEED,
    .direction = {0}
  };

  Ball b = {
    .rect = { 
      .x = 30 + PADDLE_WIDTH, 
      .y = (float)WINDOW_HEIGHT / 2 - (float)BALL_SIDES / 2, 
      .width = BALL_SIDES, 
      .height = BALL_SIDES
    },
    .color = SKYBLUE,
    .speed = BALL_SPEED,
    .spin_factor = 0.f,
    .direction = {
      .x = 1.f,
      .y = 0.f
    }
  };

  UpdateFn update = NULL;
  UdpSocket server_sock = {0};
  UdpSocket client_sock = {0};

  switch (p_cfg->game_kind) {
    case GAME_LOCAL: update = game_local_update; break;
    case GAME_NETWORK_CLIENT: {
      update = game_client_update; 
      if (!connect_to_host_udp(p_cfg->host_addr, p_cfg->host_port, &client_sock)) {
        TraceLog(LOG_FATAL, "Could not connect to the host");
      }
      net_send_cmd_wo_args(&client_sock, NET_CMD_CONNECT);
    } break;
    case GAME_NETWORK_HOST: {
      update = game_host_pending_update; 
      if (!create_udp_server_socket(p_cfg->host_port, &server_sock)) {
        TraceLog(LOG_FATAL, "Could not create a UDP server");
      }
    } break;
  }

  assert(NULL != update || "Unknown game_kind");

  GameContext ctx = {
    .paddles = {p1, p2},
    .ball = b,
    .scores = {0},
    .update = update,
    .server_sock = server_sock,
    .client_sock = client_sock,
    .pressed_key = {0},
  };

  return ctx;
}



static char *shift_args(int *argc, char ***argv) {
  if (0 == *argc) return NULL;

  char *arg = *argv[0];
  *argv += 1;
  *argc -= 1;
  return arg;
}

static CmdConfig parse_args(int argc, char **argv) {
  CmdConfig config = {0};

  config.prog = shift_args(&argc, &argv);

  while (argc > 0) {
    char *arg = shift_args(&argc, &argv);
    if (0 == strncmp(arg, "-h", 2)) {
      config.game_kind = GAME_NETWORK_HOST;

      if (argc < 1) {
        TraceLog(LOG_FATAL, "Port must be provided in command line argument: "
                 "./ping_pong -h port");
      }
      config.host_port = atoi(shift_args(&argc, &argv));
    } else if (0 == strncmp(arg, "-c", 2)) {
      config.game_kind = GAME_NETWORK_CLIENT;

      if (argc < 2) {
        TraceLog(LOG_FATAL, "Host and port must be provided in command line argument: "
                 "./ping_pong -c host port");
      }
      config.host_addr = shift_args(&argc, &argv);
      config.host_port = atoi(shift_args(&argc, &argv));
    }
  }

  return config;
}


static void game_client_update(GameContext *ctx, float dt) {
  bool try_recieve = true;
  bool enteties_updated[3] = {0};
  char buf[NET_BUF_SIZE] = {0};

  net_send_input(&ctx->client_sock, ctx->pressed_key[1]);

  while (try_recieve) {
    if (!net_recv_cmd(&ctx->client_sock, buf)) return;

    switch (buf[0]) {
      case NET_CMD_UPDATE_POSITION: {
        GameEntity e = buf[1];
        try_recieve = !enteties_updated[e];
        enteties_updated[e] = true;
        switch (e) {
          case GE_PADDLE_1: {
            ctx->paddles[0].rect.x = *(float*)&buf[2];
            ctx->paddles[0].rect.y = *(float*)&buf[2 + sizeof(float)];
          } break;
          case GE_PADDLE_2: {
            ctx->paddles[1].rect.x = *(float*)&buf[2];
            ctx->paddles[1].rect.y = *(float*)&buf[2 + sizeof(float)];
          } break;
          case GE_BALL: {
            ctx->ball.rect.x = *(float*)&buf[2];
            ctx->ball.rect.y = *(float*)&buf[2 + sizeof(float)];
          } break;
        }
      } break;
      default: TraceLog(LOG_WARNING, "Client got unknown message"); try_recieve = false;
    }
  }
}

static void game_host_pending_update(GameContext *ctx, float dt) {
  assert(ctx->client_sock.fd == 0 && "Client already has been connected");

  if (!net_check_for_connection(&ctx->server_sock, &ctx->client_sock)) {
    DrawText("Pending for a connection", WINDOW_WIDTH / 2 - 250, WINDOW_HEIGHT / 2 - 20, 40, RED);
    return;
  }

  ctx->update = game_host_update;
}

static void game_host_update(GameContext *ctx, float dt) {
  char buf[NET_BUF_SIZE] = {0};
  if (net_recv_cmd(&ctx->client_sock, buf)) {
    switch (buf[0]) {
      case NET_CMD_UPDATE_INPUT: {
        ctx->pressed_key[1] = *(int*)(buf + 1);
      } break;
    }
  }

  handle_pressed_key(ctx, 0);
  handle_pressed_key(ctx, 1);

  game_local_update(ctx, dt);

  net_send_position(&ctx->client_sock, GE_PADDLE_1, ctx->paddles[0].rect.x, ctx->paddles[0].rect.y);
  net_send_position(&ctx->client_sock, GE_PADDLE_2, ctx->paddles[1].rect.x, ctx->paddles[1].rect.y);
  net_send_position(&ctx->client_sock, GE_BALL, ctx->ball.rect.x, ctx->ball.rect.y);
}

static void game_local_update(GameContext *ctx, float dt) {
  if (ctx->ball.rect.x >= WINDOW_WIDTH - ctx->ball.rect.width) {
    ctx->scores[0] += 1;

    ctx->paddles[0].rect.y = (float)WINDOW_HEIGHT / 2 - (float)PADDLE_HEIGHT / 2;
    ctx->paddles[1].rect.y = (float)WINDOW_HEIGHT / 2 - (float)PADDLE_HEIGHT / 2;

    ctx->ball.rect.x = ctx->paddles[0].rect.x + PADDLE_WIDTH;
    ctx->ball.rect.y = ctx->paddles[0].rect.y + ctx->paddles[0].rect.height / 2 - (float)BALL_SIDES / 2;
    ctx->ball.direction.x = 1;
    ctx->ball.direction.y = 0.f;
    ctx->ball.color = ctx->paddles[0].color;
    ctx->ball.speed = BALL_SPEED;
    ctx->ball.spin_factor = 0.f;
  }

  if (ctx->ball.rect.x <= 0) {
    ctx->scores[1] += 1;

    ctx->paddles[0].rect.y = (float)WINDOW_HEIGHT / 2 - (float)PADDLE_HEIGHT / 2;
    ctx->paddles[1].rect.y = (float)WINDOW_HEIGHT / 2 - (float)PADDLE_HEIGHT / 2;

    ctx->ball.rect.x = ctx->paddles[1].rect.x - PADDLE_WIDTH;
    ctx->ball.rect.y = ctx->paddles[1].rect.y + ctx->paddles[1].rect.height / 2 - (float)BALL_SIDES / 2;
    ctx->ball.direction.x = -1;
    ctx->ball.direction.y = 0.f;
    ctx->ball.color = ctx->paddles[1].color;
    ctx->ball.speed = BALL_SPEED;
    ctx->ball.spin_factor = 0.f;
  }

  if (ctx->ball.rect.y <= 0 || ctx->ball.rect.y >= WINDOW_HEIGHT - ctx->ball.rect.height) {
    ctx->ball.direction.y *= -1;
    ctx->ball.speed = Clamp(ctx->ball.speed * 0.9f, MIN_BALL_SPEED, MAX_BALL_SPEED);
  }

  if (CheckCollisionRecs(ctx->ball.rect, ctx->paddles[0].rect)) {
    handle_collision(&ctx->ball, &ctx->paddles[0]);
    ctx->ball.rect.x = ctx->paddles[0].rect.x + ctx->paddles[0].rect.width; // Move ball to avoid sticking
  }

  if (CheckCollisionRecs(ctx->ball.rect, ctx->paddles[1].rect)) {
    handle_collision(&ctx->ball, &ctx->paddles[1]);
    ctx->ball.rect.x = ctx->paddles[1].rect.x - ctx->paddles[1].rect.width; // Move ball to avoid sticking
  }

  ctx->paddles[0].rect.y += ctx->paddles[0].speed * ctx->paddles[0].direction.y * dt;
  ctx->paddles[1].rect.y += ctx->paddles[1].speed * ctx->paddles[1].direction.y * dt;

  ctx->ball.direction = Vector2Normalize(ctx->ball.direction);
  ctx->ball.rect.x += ctx->ball.speed * ctx->ball.direction.x * dt;
  ctx->ball.rect.y += ctx->ball.speed * ctx->ball.direction.y * dt;

  clamp_rect_within_screen(&ctx->paddles[0].rect);
  clamp_rect_within_screen(&ctx->paddles[1].rect);
  clamp_rect_within_screen(&ctx->ball.rect);
}

static void game_draw_ui(GameContext *ctx, float dt) {
  char buf[1024] = {0};
  int stats_font_size = 14;

  sprintf(buf, "Speed: %.0f", ctx->paddles[0].speed);
  DrawText(buf, 30, WINDOW_HEIGHT - 30, stats_font_size, MAIN_UI_COLOR);

  sprintf(buf, "Speed: %.0f", ctx->paddles[1].speed);
  int speed1_width = MeasureText(buf, stats_font_size);
  int speed1_pos_x = WINDOW_WIDTH - speed1_width - 30;
  DrawText(buf, speed1_pos_x, WINDOW_HEIGHT - 30, stats_font_size, MAIN_UI_COLOR);

  sprintf(buf, "Ball Speed: %.0f", ctx->ball.speed);
  int ball_speed_width = MeasureText(buf, stats_font_size);
  int ball_center_x = (WINDOW_WIDTH - ball_speed_width) / 2;
  DrawText(buf, ball_center_x, WINDOW_HEIGHT - 30, stats_font_size, MAIN_UI_COLOR);

  sprintf(buf, "FPS: %d", GetFPS());
  int fps_width = MeasureText(buf, stats_font_size);
  DrawText(buf, WINDOW_WIDTH - fps_width - 60, 30, stats_font_size, MAIN_UI_COLOR);
}

static void draw_score(GameContext *ctx, float dt) {
  char buf[1024] = {0};
  int score_font_size = 150;

  Color color = MAIN_UI_COLOR;
  color.a = 70;

  sprintf(buf, "%d", ctx->scores[0]);
  int score_width = MeasureText(buf, score_font_size);
  int score_center_x = (WINDOW_WIDTH - score_width) / 4;
  DrawText(buf, score_center_x, (WINDOW_HEIGHT - score_font_size) / 2, score_font_size, color);

  sprintf(buf, "%d", ctx->scores[1]);
  score_width = MeasureText(buf, score_font_size);
  score_center_x = WINDOW_WIDTH - (WINDOW_WIDTH - score_width) / 4 - score_width;
  DrawText(buf, score_center_x, (WINDOW_HEIGHT - score_font_size) / 2, score_font_size, color);
}


static void draw_tail(GameContext *ctx, Rectangle orig_rect, Color orig_color,
                      Vector2 *p_tail, int *p_begin, int *p_len, int tail_capacity) {
  if (!ctx->is_paused) {
    p_tail[*p_begin] = (Vector2){ orig_rect.x + orig_rect.width / 2, orig_rect.y + orig_rect.height / 2 };
    *p_begin = (*p_begin + 1) % tail_capacity;
    *p_len += *p_len != tail_capacity;
  }

  int step = tail_capacity / 5;

  for (
    int curr = *p_begin, i = 0; 
    i < *p_len; 
    curr = (curr + step) % tail_capacity, i += step
  ) {
    Color color = orig_color;
    color.a = Lerp(color.a, 0, 1 - (float)i / tail_capacity);
    float w = Lerp(orig_rect.width, orig_rect.width / tail_capacity, 1 - (float)i / tail_capacity);
    float h = Lerp(orig_rect.height, orig_rect.height / tail_capacity, 1 - (float)i / tail_capacity);
    DrawRectangleLines(p_tail[curr].x - w / 2, p_tail[curr].y - h / 2, w, h, color);
  }
}

static void game_draw_frame(GameContext *ctx, float dt) {
  Rectangle middle_line = {0};
  middle_line.width = 5;
  middle_line.height = WINDOW_HEIGHT;
  middle_line.x = (WINDOW_WIDTH - middle_line.width) / 2;
  middle_line.y = 0;

  BeginDrawing();

  ClearBackground(BACKGROUND_COLOR);

  draw_score(ctx, dt);

  float line_thickness = 2;
  DrawRectangleRec(middle_line, CLITERAL(Color){ 255, 255, 255, 100 });
  DrawRectangleLinesEx(ctx->paddles[0].rect, line_thickness, ctx->paddles[0].color);
  DrawRectangleLinesEx(ctx->paddles[1].rect, line_thickness, ctx->paddles[1].color);
  DrawRectangleLinesEx(ctx->ball.rect, line_thickness, ctx->ball.color);

  draw_tail(ctx, ctx->ball.rect, ctx->ball.color, 
            ctx->ball.tail, &ctx->ball.tail_begin, &ctx->ball.tail_len, TAIL_CAPACITY_BALL);

  if (ctx->paddles[0].direction.y != 0) {
    draw_tail(ctx, ctx->paddles[0].rect, ctx->paddles[0].color, 
              ctx->paddles[0].tail, &ctx->paddles[0].tail_begin, &ctx->paddles[0].tail_len, TAIL_CAPACITY_PADDLE);
  }

  if (ctx->paddles[1].direction.y != 0) {
    draw_tail(ctx, ctx->paddles[1].rect, ctx->paddles[1].color, 
              ctx->paddles[1].tail, &ctx->paddles[1].tail_begin, &ctx->paddles[1].tail_len, TAIL_CAPACITY_PADDLE);
  }

  game_draw_ui(ctx, dt);
  
  EndDrawing();
}


int main(int argc, char **argv)
{
  CmdConfig config = parse_args(argc, argv);

  const char *window_name = NULL;
  switch (config.game_kind) {
    case GAME_LOCAL: window_name = "PingPong (Local)"; break;
    case GAME_NETWORK_HOST: window_name = "PingPong (Host)"; break;
    case GAME_NETWORK_CLIENT: window_name = "PingPong (Client)"; break;
  }
  assert(NULL != window_name);

  //SetTargetFPS(60);
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, window_name);

  GameContext ctx = game_init(&config);

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    if (IsKeyPressed(KEY_SPACE)) {
      ctx.is_paused = !ctx.is_paused;
    } 

    if (!ctx.is_paused) {
      handle_input(&ctx, dt);
      ctx.update(&ctx, dt);
    }
    game_draw_frame(&ctx, dt);
  }

  CloseWindow();

  return 0;
}
