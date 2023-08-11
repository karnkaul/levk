project(spaced)

add_executable(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE levk::lib levk::options)

target_include_directories(${PROJECT_NAME} PRIVATE .)

target_sources(${PROJECT_NAME} PRIVATE
  spaced/game/actor.hpp
  spaced/game/blueprint.cpp
  spaced/game/blueprint.hpp
  spaced/game/collision_channel.hpp
  spaced/game/control_scheme.cpp
  spaced/game/control_scheme.hpp
  spaced/game/game.cpp
  spaced/game/game.hpp
  spaced/game/hit_points.hpp

  spaced/actors/asteroid.cpp
  spaced/actors/asteroid.hpp
  spaced/actors/player.cpp
  spaced/actors/player.hpp
  spaced/actors/projectile.hpp

  spaced/blueprints/asteroid.cpp
  spaced/blueprints/asteroid.hpp
  spaced/blueprints/player.cpp
  spaced/blueprints/player.hpp
  spaced/blueprints/projectile.cpp
  spaced/blueprints/projectile.hpp

  spaced/components/cannon.cpp
  spaced/components/cannon.hpp
  spaced/components/displacer.cpp
  spaced/components/displacer.hpp
  spaced/components/health_bar.cpp
  spaced/components/health_bar.hpp
  spaced/components/hit_target.cpp
  spaced/components/hit_target.hpp
  spaced/components/player_controller.cpp
  spaced/components/player_controller.hpp
  spaced/components/spawner.cpp
  spaced/components/spawner.hpp
  spaced/components/tumbler.cpp
  spaced/components/tumbler.hpp
  spaced/components/turret.cpp
  spaced/components/turret.hpp

  spaced/app.cpp
  spaced/app.hpp
  spaced/inclusive_range.hpp
  spaced/layout.hpp

  main.cpp
)
