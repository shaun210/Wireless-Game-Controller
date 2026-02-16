import pygame
import random
import asyncio
import math
import time
from bleak import BleakClient

# --- BLE Configuration ---
DEVICE_ADDRESS = "49C13C3B-DA8C-36A7-541A-D492FA1FCBDA" 
CHARACTERISTIC_UUID = "00005678-0000-1000-8000-00805f9b34fb"

# Global variables
current_command = "stop"
last_packet_time = 0
packet_interval_str = "Waiting..."

# --- Constants ---
BLACK = (0, 0, 0)
WHITE = (220, 220, 255)
PLAYER_COLOR = (0, 255, 150)
ENEMY_COLOR = (255, 50, 50)
BULLET_COLOR = (255, 200, 50)

SCREEN_WIDTH = 600
SCREEN_HEIGHT = 700
NUMBER_OF_ENEMIES = 6
STAR_COUNT = 100
ENEMY_WIDTH = 50
ENEMY_HEIGHT = 40
ENEMY_BULLET_SPEED = 3
ENEMY_FIRE_DELAY = 1000
ENEMY_FIRE_CHANCE = 0.3
PLAYER_HEALTH = 1000
PLAYER_SPEED = 15

enemies = []
player_bullets = []
enemy_bullets = []
stars = []

# --- BLE Notification Handler ---
def notification_handler(sender, data):
    """
    Handles incoming data from STM32.
    Calculates time since the LAST packet to measure 'Update Rate'.
    """
    global current_command, last_packet_time, packet_interval_str
    
    current_time = time.perf_counter()
    
    # --- MEASURE INTERVAL ---
    if last_packet_time != 0:
        diff_ms = (current_time - last_packet_time) * 1000
        # Update the display string
        packet_interval_str = f"Update Rate: {diff_ms:.1f}ms"
    
    last_packet_time = current_time

    # --- DECODE COMMAND ---
    try:
        val = int.from_bytes(data, byteorder='little')
        if val == 0:
            current_command = "stop"
        elif val == 1:
            current_command = "left"
        elif val == 2:
            current_command = "right"
        elif val == 3:
            current_command = "fire"
            
    except Exception as e:
        print(f"Error decoding BLE data: {e}")

# --- Star Field ---
def generate_stars():
    for _ in range(STAR_COUNT):
        stars.append({
            'x': random.randint(0, SCREEN_WIDTH),
            'y': random.randint(0, SCREEN_HEIGHT),
            'radius': random.randint(1, 2)
        })

def draw_stars(screen):
    for star in stars:
        color = (200, 200, 255) if star['radius'] == 1 else WHITE
        pygame.draw.circle(screen, color, (star['x'], star['y']), star['radius'])

# --- Explosion Class ---
class Explosion:
    def __init__(self, x, y):
        self.x = x
        self.y = y
        self.particles = []
        self.life_time = 45
        self.current_frame = 0
        for _ in range(15):
            angle = random.uniform(0, 2 * math.pi)
            speed = random.uniform(2, 5)
            self.particles.append({
                'x': x,
                'y': y,
                'vx': speed * math.cos(angle),
                'vy': speed * math.sin(angle),
                'size': random.randint(3, 6),
                'color': random.choice([(255, 165, 0), (255, 50, 0), (255, 255, 0)])
            })

    def update(self):
        self.current_frame += 1
        for p in self.particles:
            p['x'] += p['vx']
            p['y'] += p['vy']
            p['vx'] *= 0.95
            p['vy'] *= 0.95
            p['size'] = max(0, p['size'] - 0.1)

    def draw(self, screen):
        if self.current_frame < self.life_time:
            alpha = max(0, 255 - int(255 * (self.current_frame / self.life_time) * 1.5))
            surface = pygame.Surface((60, 60), pygame.SRCALPHA)
            fire_color = (255, 100, 0, alpha)
            pygame.draw.circle(surface, fire_color, (30, 30), 25)
            screen.blit(surface, (self.x - 30, self.y - 30))
            for p in self.particles:
                if p['size'] > 0:
                    pygame.draw.circle(screen, p['color'], (int(p['x']), int(p['y'])), int(p['size']))

# --- Image Creation ---
def create_player_image():
    size = (50, 40)
    img = pygame.Surface(size, pygame.SRCALPHA)
    pygame.draw.polygon(img, (0, 150, 80), [(0, 40), (50, 40), (25, 0)])
    pygame.draw.polygon(img, PLAYER_COLOR, [(20, 20), (30, 20), (25, 0)])
    pygame.draw.rect(img, (255, 100, 0), (10, 35, 10, 5))
    pygame.draw.rect(img, (255, 100, 0), (30, 35, 10, 5))
    return img

def create_enemy_image():
    img = pygame.Surface((ENEMY_WIDTH, ENEMY_HEIGHT), pygame.SRCALPHA)
    pygame.draw.polygon(img, ENEMY_COLOR, [(5, 10), (45, 10), (50, 30), (0, 30)])
    pygame.draw.rect(img, (150, 0, 0), (0, 30, 50, 10))
    pygame.draw.circle(img, (255, 255, 0), (15, 20), 4)
    pygame.draw.circle(img, (255, 255, 0), (35, 20), 4)
    return img

def create_bullet_image():
    img = pygame.Surface((8, 20), pygame.SRCALPHA)
    pygame.draw.rect(img, (255, 200, 100, 150), (0, 0, 8, 20))
    pygame.draw.rect(img, BULLET_COLOR, (2, 0, 4, 20))
    return img

# --- Player Class ---
class Player:
    def __init__(self):
        self.image = create_player_image()
        self.rect = self.image.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT - 100))
        self.health = PLAYER_HEALTH
        self.points = 0
        self.speed = PLAYER_SPEED
        self.alive = True

    def move(self, direction):
        if direction == "left":
            self.rect.x -= self.speed
        elif direction == "right":
            self.rect.x += self.speed
        self.rect.x = max(0, min(self.rect.x, SCREEN_WIDTH - self.rect.width))

    def draw(self, screen):
        if self.alive:
            screen.blit(self.image, self.rect)

    def hit(self, damage=25):
        self.health -= damage
        if self.health <= 0:
            self.alive = False

    def add_points(self, amount):
        self.points += amount
    
    def is_player_alive(self):
        return self.alive

# --- Enemy/Bullet Logic ---
def spawn_enemy():
    enemy_img = create_enemy_image()
    while True:
        enemy_rect = enemy_img.get_rect()
        enemy_rect.x = random.randint(10, SCREEN_WIDTH - ENEMY_WIDTH - 10)
        enemy_rect.y = random.randint(50, 150)
        if not any(enemy_rect.colliderect(e['rect']) for e in enemies):
            enemies.append({'img': enemy_img, 'rect': enemy_rect})
            break

def spawn_bullet(x, y, shooter):
    bullet_img = create_bullet_image()
    bullet_rect = bullet_img.get_rect(center=(x, y))
    if shooter == 'player':
        player_bullets.append({'img': bullet_img, 'rect': bullet_rect})
    elif shooter == 'enemy':
        enemy_bullets.append({'img': bullet_img, 'rect': bullet_rect})

def draw_player_stats(screen, player, FONT):
    box_width = 180
    box_height = 80 
    panel_color = (0, 0, 50, 150)
    panel = pygame.Surface((box_width, box_height), pygame.SRCALPHA)
    panel.fill(panel_color)

    health_text = FONT.render(f"Health: {player.health}", True, (255, 50, 50))
    points_text = FONT.render(f"Points: {player.points}", True, (255, 200, 50))
    
    # Interval Text
    lat_text = FONT.render(packet_interval_str, True, (0, 255, 255))

    panel.blit(health_text, (10, 10))
    panel.blit(points_text, (10, 30))
    panel.blit(lat_text, (10, 55)) 
    screen.blit(panel, (SCREEN_WIDTH - box_width - 10, SCREEN_HEIGHT - box_height - 10))

def game_ends_screen(screen):
    font_big = pygame.font.Font(None, 80)
    font_small = pygame.font.Font(None, 36)
    text = font_big.render("YOU ARE DEAD", True, (255, 50, 50))
    subtext = font_small.render("Press ESC to Quit", True, (255, 200, 200))

    scale = 1 + 0.05 * math.sin(pygame.time.get_ticks() * 0.005)
    new_size = (int(text.get_width() * scale), int(text.get_height() * scale))
    text_scaled = pygame.transform.smoothscale(text, new_size)

    text_rect = text_scaled.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2))
    sub_rect = subtext.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2 + 60))

    glow_surface = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT), pygame.SRCALPHA)
    glow_intensity = int(50 + 50 * abs(math.sin(pygame.time.get_ticks() * 0.01)))
    pygame.draw.circle(glow_surface, (255, 0, 0, glow_intensity),
                       (SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2), 150)
    screen.blit(glow_surface, (0, 0))

    screen.blit(text_scaled, text_rect)
    screen.blit(subtext, sub_rect)


# --- ASYNC Main Game Loop ---
async def start_game(client):
    global current_command
    
    pygame.init()
    screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
    pygame.display.set_caption("Neon Space Invaders (Interval Monitor)")
    clock = pygame.time.Clock()
    pygame.font.init()
    FONT = pygame.font.Font(None, 28)

    generate_stars()
    player = Player()

    for _ in range(NUMBER_OF_ENEMIES):
        spawn_enemy()

    explosions = []
    running = True
    last_enemy_fire_time = 0

    print("Game Started!")
    print("Controls: BLE Controller")

    while running:
        await asyncio.sleep(0) 
        clock.tick(60)
        screen.fill(BLACK)
        draw_stars(screen)

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False

        # --- Read Global Command ---
        if current_command == "left":
            player.move("left")
        elif current_command == "right":
            player.move("right")
        elif current_command == "fire":
            if len(player_bullets) == 0 or player_bullets[-1]['rect'].y < player.rect.y - 50:
                spawn_bullet(player.rect.centerx, player.rect.y, 'player')
            current_command = "stop" 
        elif current_command == "stop":
            pass 

        # --- Game Physics ---
        active_player_bullets = []
        for bullet in player_bullets:
            bullet['rect'].y -= 10
            if bullet['rect'].y > 0:
                hit_enemy_index = None
                for i, enemy in enumerate(enemies):
                    if bullet['rect'].colliderect(enemy['rect']):
                        explosions.append(Explosion(enemy['rect'].centerx, enemy['rect'].centery))
                        hit_enemy_index = i
                        player.add_points(10)
                        break
                if hit_enemy_index is not None:
                    enemies.pop(hit_enemy_index)
                else:
                    active_player_bullets.append(bullet)
                    screen.blit(bullet['img'], bullet['rect'])
        player_bullets[:] = active_player_bullets

        # Enemy Bullets
        active_enemy_bullets = []
        for bullet in enemy_bullets:
            bullet['rect'].y += ENEMY_BULLET_SPEED
            if bullet['rect'].y < SCREEN_HEIGHT:
                if bullet['rect'].colliderect(player.rect) and player.alive:
                    explosions.append(Explosion(player.rect.centerx, player.rect.centery))
                    player.hit()
                else:
                    active_enemy_bullets.append(bullet)
                    screen.blit(bullet['img'], bullet['rect'])
        enemy_bullets[:] = active_enemy_bullets

        # Enemy Firing
        current_time = pygame.time.get_ticks()
        difficulty_scale = max(0.5, 1 - (player.points // 50) * 0.05)
        dynamic_fire_delay = int(ENEMY_FIRE_DELAY * difficulty_scale)

        if current_time - last_enemy_fire_time > dynamic_fire_delay:
            for enemy in enemies:
                if random.random() < ENEMY_FIRE_CHANCE:
                    spawn_bullet(enemy['rect'].centerx, enemy['rect'].y + ENEMY_HEIGHT, 'enemy')
            last_enemy_fire_time = current_time

        while len(enemies) < NUMBER_OF_ENEMIES:
            spawn_enemy()

        active_explosions = []
        for exp in explosions:
            exp.update()
            exp.draw(screen)
            if exp.current_frame < exp.life_time:
                active_explosions.append(exp)
        explosions[:] = active_explosions

        for enemy in enemies:
            screen.blit(enemy['img'], enemy['rect'])

        if player.is_player_alive():
            player.draw(screen)
        else:
            game_ends_screen(screen)

        draw_player_stats(screen, player, FONT)
        pygame.display.update()

    pygame.quit()

async def main():
    print(f"Searching for device: {DEVICE_ADDRESS}...")
    async with BleakClient(DEVICE_ADDRESS) as client:
        print(f"Connected to {DEVICE_ADDRESS}")
        await client.start_notify(CHARACTERISTIC_UUID, notification_handler)
        await start_game(client)
        await client.stop_notify(CHARACTERISTIC_UUID)

if __name__ == "__main__":
    asyncio.run(main())