# FlashHelper Architecture

## Module Boundaries

- **MainWindow**: Focuses on UI binding, user input, and high-level feedback. It uses controllers to execute operations.
- **FlashOperationController**: Manages the lifecycle of a `QProcess` that executes `flashrom` or the local helper. It handles state transitions and accumulates output/errors.
- **LocalFlashManager**: Orchestrates local SPI flash operations (Detect/Read/Write). It uses the `FlashOperationController` for execution but handles the local-specific output parsing and logic.
- **SmartMerge**: Implements the logic for "Smart Merge" (partial write). It calculates the merge, prepares the layout file, and handles the file-level merge operation.
- **LocalSpiDriver**: Manages the low-level SPI communication for local flashing (direct MMIO on Loongson platforms).

## Privilege Model

FlashHelper uses a "Helper + pkexec" model for security:
1. The main GUI (FlashHelper) runs as a normal user.
2. When low-level hardware access is needed (e.g., direct MMIO or `flashrom`), it calls `pkexec` to execute either `flashrom` or the bundled `flashhelper-helper`.
3. The `flashhelper-helper` binary is small and focused on direct MMIO access, reducing the attack surface.

## Smart Write Flow

1. The user attempts to write an image to a flash chip.
2. If the image is smaller than the chip capacity, `flashrom` (or the helper) detects a size mismatch and fails.
3. FlashHelper catches this failure, parses the error to get the chip size, and offers a "Smart Merge":
   - **Step 1: Read**: The original flash content is read into a temporary file.
   - **Step 2: Merge**: The new (smaller) image is merged with the "tail" of the original flash content to create a full-sized image.
   - **Step 3: Write (Partial)**: A `flashrom` layout file is generated to only write the modified portion (the head) of the flash, preserving the rest and saving time.
