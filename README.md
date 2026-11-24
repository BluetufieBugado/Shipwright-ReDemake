<img width="480" alt="Logo Zelda Ocarina of Time ReDemake" src="https://github.com/user-attachments/assets/a2acba5b-8980-43ac-9aa9-dd95bd5fd52e" />

# What is this project?

This is a small fork of the original [*Ship of Harkinian*](https://github.com/HarbourMasters/Shipwright), created with the goal of bringing elements of the remake experience to the classic version already available on PC—offering a refreshed experience, albeit with more limited features.

The intention is not to recreate the remake entirely, but rather to incorporate specific features exclusive to the remake and introduce a more modern feel to the classic game.

## What is the current focus of the work? (11/24/26)

The rendering system is being reworked to introduce modern visual effects—such as color correction, bloom, and true dynamic lighting—into the game. So far, these three features have already been implemented, and I am currently exploring the possibility of adding realistic shadows, similar to those found in modern games. It is an ambitious step, but one I am keen to attempt—and that is exactly what I have been working on. Furthermore, this process has helped me gain a better understanding of the game’s rendering engine.


<img width="480" alt="Hyrule Field com efeitos ligados" src="https://github.com/user-attachments/assets/0a0e65a3-0175-4a7d-92ba-e96d507fc7b6" />

### What has already been implemented in this phase?

Color correction ✔

Bloom ✔

Dynamic light points (the lights actually illuminate the environment and objects, rather than being just static points.) ✔


## What is currently working?


✔ Faster movement speed after rolling


✔ A separate, dedicated jump button for Link (press the C-down button)

<img width="480" alt="image" src="https://github.com/user-attachments/assets/5241d09f-b444-4567-ae72-932a8cad4a7c" />


✔ Songs partially replaced, thanks to the "Future" tool developed by the community


✔ Pause menus redesigned to be identical to those in the Remake


<img width="480" alt="image" src="https://github.com/user-attachments/assets/0d567e51-0901-4837-9494-ec7e9d04d708" />
<img width="480" alt="image" src="https://github.com/user-attachments/assets/fb476105-8e8f-4acb-af94-8bd779d58923" />

- Items ✔
- Equipment ✔
- Quest Items ✔
- Map ✔

The menus are almost ready, but they still need some minor fine-tuning in the programming.

✔ MicOcarina is working perfectly, detecting notes that are played or hummed in various ways. The "song-guessing" system found in the Remake has been partially replicated here; it is not 100% accurate—much like in the Remake, where you might hum a melody and the game still fails to correctly identify the specific song you have in mind.


<img width="480" alt="image" src="https://github.com/user-attachments/assets/8673a133-e705-475d-81ec-e0ae39d45243" />

✔ Music Notebook when you open the ocarina and press the down button on the D-pad (based on the Nintendo 64 controller, not the C down button)

<img width="480" alt="image" src="https://github.com/user-attachments/assets/9fe71224-9609-4cc8-b119-db1c963e6c10" />

✔ Aiming with the slingshot, as well as with the bow and arrow, in third person—just like in the Remake.

<img width="480" alt="image" src="https://github.com/user-attachments/assets/1946b7a7-f87d-4057-aade-61ceca294392" />


✔ Faster weapon and equipment switching during gameplay
Press the C-Left or C-Right buttons while holding the opposite button to quickly switch items during gameplay—much like in *Breath of the Wild*, *Tears of the Kingdom*, and the recent remake for the Switch 2. These buttons are mapped to the controller's triggers when using the control preset found under "Esc > ReDemake > Controls > Gamepad Preset." Press the C-Up button to equip the highlighted item.

<img width="480" alt="image" src="https://github.com/user-attachments/assets/6cb84e7a-0870-4d5d-abd1-17c23f577ce8" />

To adjust the arrangement of items, press the button on your controller that corresponds to the N64's B button.

❌ Smoother controls

❌ Maps and environments remain identical to the original game and require reworking

❌ Ragdoll physics (not sure if this might be added in the future; it remains a possibility—a "maybe")

## Known issues

The tests conducted so far, which modify aspects of the renderer, were performed using DirectX11; consequently, bugs or inconsistencies may arise when switching to OpenGL. One example of this is the “Link” feature in the equipment menu, which disappears entirely when using OpenGL—a flaw that should be addressed in future versions.

The graphical changes aimed at improving the game's quality are experimental and entirely new. It has not yet been determined whether these might negatively impact the game at some point; the same applies to other additional mechanics that have been introduced. However, as far as I am aware, there do not appear to be any issues at this time.

Furthermore, all testing was carried out exclusively on the Windows version, as that is the operating system I use. If you encounter any problems on Linux or macOS, I would appreciate your help in reporting them via the issue tracker, and I will do my best to fix them as quickly as possible!

## Android? 

An Android version is a strong possibility; it will likely be developed in a separate repository, building upon the work of [Linkzenic](https://github.com/linkzenic/Shipwright-Ports).

I am not yet familiar with the code structure of the Android version of SoH, but if it follows the same logic, porting the features found in the "Enhancements" folder here might not be too difficult—though only time will tell. The renderer may well be the biggest challenge, as it almost certainly required some adaptation for that platform; that will be a separate task. My intention is to aim for a simultaneous release: once the current build reaches a satisfactory state, I will work on the Android version so that it can be launched alongside the Windows, Mac, and Linux versions.

