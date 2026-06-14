import unreal

"""
Расширение тестовой локации для SCP проекта.
Запустить в UE: Edit > Editor Preferences > Plugins > Python > Restart
Затем: Window > Developer Tools > Output Console > Execute Python Script
Или: Tools > Execute Python Script
"""

# =============================================
# НАСТРОЙКИ
# =============================================

# Размеры коридора (в см)
CORRIDOR_WIDTH = 400      # ширина коридора
CORRIDOR_HEIGHT = 300     # высота потолка
CORRIDOR_LENGTH = 800     # длина коридора

# Размеры комнаты
ROOM_WIDTH = 600          # ширина комнаты
ROOM_LENGTH = 600         # длина комнаты
ROOM_HEIGHT = 300         # высота

# Лестница
STAIR_WIDTH = 300         # ширина лестницы
STAIR_DEPTH = 30          # глубина ступени
STAIR_HEIGHT = 20         # высота ступени
STAIR_COUNT = 10          # количество ступеней

# =============================================
# СОЗДАНИЕ ГЕОМЕТРИИ
# =============================================

def create_brush(name, location, size, brush_type="ADDITIVE"):
    """Создаёт BSP brush"""
    actor_class = unreal.WorldEditorSubsystem if hasattr(unreal, 'WorldEditorSubsystem') else None
    
    # Используем EditorActorSubsystem
    editor_actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    
    # Создаём brush actor
    actor = editor_actor.spawn_actor_from_class(
        unreal.Brush,
        location,
        unreal.Rotator(0, 0, 0)
    )
    
    if actor:
        actor.set_actor_label(name)
        
        # Получаем brush component
        brush_comp = actor.get_brush_component()
        if brush_comp:
            # Устанавливаем размер
            brush = brush_comp.get_brush()
            if brush:
                brush.box.brush = unreal.BrushBuilder.create_box_brush(
                    size.x, size.y, size.z
                )
        
        unreal.log(f"Создан: {name} в {location}")
        return actor
    return None

def create_corridor(start_x, start_y, start_z, length_x, width_y, height_z, name="Corridor"):
    """Создаёт коридор из 5 стен + пол + потолок"""
    actors = []
    
    # Пол
    floor_loc = unreal.Vector(
        start_x + length_x/2,
        start_y + width_y/2,
        start_z
    )
    a = create_brush(f"{name}_Floor", floor_loc, unreal.Vector(length_x, width_y, 20))
    if a: actors.append(a)
    
    # Потолок
    ceil_loc = unreal.Vector(
        start_x + length_x/2,
        start_y + width_y/2,
        start_z + height_z
    )
    a = create_brush(f"{name}_Ceiling", ceil_loc, unreal.Vector(length_x, width_y, 20))
    if a: actors.append(a)
    
    # Левая стена
    left_loc = unreal.Vector(
        start_x + length_x/2,
        start_y + 10,
        start_z + height_z/2
    )
    a = create_brush(f"{name}_LeftWall", left_loc, unreal.Vector(length_x, 20, height_z))
    if a: actors.append(a)
    
    # Правая стена
    right_loc = unreal.Vector(
        start_x + length_x/2,
        start_y + width_y - 10,
        start_z + height_z/2
    )
    a = create_brush(f"{name}_RightWall", right_loc, unreal.Vector(length_x, 20, height_z))
    if a: actors.append(a)
    
    # Задняя стена
    back_loc = unreal.Vector(
        start_x + 10,
        start_y + width_y/2,
        start_z + height_z/2
    )
    a = create_brush(f"{name}_BackWall", back_loc, unreal.Vector(20, width_y, height_z))
    if a: actors.append(a)
    
    return actors

def create_staircase(start_x, start_y, start_z, width, step_depth, step_height, count, name="Stairs"):
    """Создаёт лестницу из ступеней"""
    actors = []
    
    for i in range(count):
        step_loc = unreal.Vector(
            start_x + (step_depth * i) + step_depth/2,
            start_y + width/2,
            start_z + (step_height * i) + step_height/2
        )
        a = create_brush(
            f"{name}_Step_{i}",
            step_loc,
            unreal.Vector(step_depth, width, step_height)
        )
        if a: actors.append(a)
    
    return actors

# =============================================
# ГЛАВНАЯ ПРОГРАММА
# =============================================

print("=" * 50)
print("РАСШИРЕНИЕ ТЕСТОВОЙ ЛОКАЦИИ SCP")
print("=" * 50)

# Позиция начала (подстрой под свою карту)
START_X = 0
START_Y = 0
START_Z = 0

# 1. Создаём扩展ённый коридор (длиннее)
print("\n1. Создаю扩展ённый корridor...")
corridor_actors = create_corridor(
    START_X, START_Y, START_Z,
    CORRIDOR_LENGTH * 2,  # удлиннённый
    CORRIDOR_WIDTH,
    CORRIDOR_HEIGHT,
    "Extended_Corridor"
)

# 2. Создаём лестницу в конце коридора
print("\n2. Создаю лестницу...")
stair_x = START_X + CORRIDOR_LENGTH * 2 + 100
stair_actors = create_staircase(
    stair_x,
    START_Y + CORRIDOR_WIDTH/2 - STAIR_WIDTH/2,
    START_Z,
    STAIR_WIDTH,
    STAIR_DEPTH,
    STAIR_HEIGHT,
    STAIR_COUNT,
    "Stairs"
)

# 3. Создаём комнату наверху лестницы
print("\n3. Создаю большую комнату...")
room_z = START_Z + (STAIR_HEIGHT * STAIR_COUNT)  # высота после лестницы
room_actors = create_corridor(
    stair_x + STAIR_DEPTH * STAIR_COUNT + 50,
    START_Y - 100,  # чуть шире
    room_z,
    ROOM_LENGTH,
    ROOM_WIDTH,
    ROOM_HEIGHT,
    "Large_Room"
)

# 4. Создаём проём в стене (вырез в стене коридора)
print("\n4. Создаю проём для лестницы...")
# Вырезаем часть стены, чтобы войти на лестницу
cut_loc = unreal.Vector(
    stair_x - 50,
    START_Y + CORRIDOR_WIDTH/2,
    START_Z + CORRIDOR_HEIGHT/2
)
cut_actor = create_brush(
    "Wall_Cutout",
    cut_loc,
    unreal.Vector(100, 150, 200)
)

# Итого
total = len(corridor_actors) + len(stair_actors) + len(room_actors) + 1
print(f"\n{'=' * 50}")
print(f"ГОТОВО! Создано объектов: {total}")
print(f"  - Extended Corridor: {CORRIDOR_LENGTH*2}x{CORRIDOR_WIDTH}x{CORRIDOR_HEIGHT}")
print(f"  - Stairs: {STAIR_COUNT} ступеней")
print(f"  - Large Room: {ROOM_WIDTH}x{ROOM_LENGTH}x{ROOM_HEIGHT}")
print(f"{'=' * 50}")
print("\nНе забудь:")
print("  1. Выделить все созданные объекты")
print("  2. Сгруппировать (Ctrl+G)")
print("  3. Сохранить карту (Ctrl+S)")
print("  4. Добавить освещение!")
