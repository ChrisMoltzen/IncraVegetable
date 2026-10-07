# Tech tree backups

`TechTreeData-before-split.h` is the tech tree as it was before every stacking
tech (max level above 1) was split into one tech per level. To go back to it:

    cp backups/TechTreeData-before-split.h include/TechTreeData.h
    make

(or `git checkout tech-tree-before-split -- include/TechTreeData.h`).

Saves carry over in both directions: the game reads an old "value 3" as
Prize Produce 1-3 bought, and a split-tree "value_3" as Prize Produce level 3.
