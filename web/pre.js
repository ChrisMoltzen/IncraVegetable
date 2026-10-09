// web/pre.js - runs before the game starts in a browser (make web / make demo-web).
// SDL keeps the save folder under /libsdl. Back it with IndexedDB so saves
// survive closing the tab, and wait for the stored copy to load first.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  FS.mkdir('/libsdl');
  FS.mount(IDBFS, {}, '/libsdl');
  addRunDependency('incra-saves');
  FS.syncfs(true, function (err) {
    if (err) console.warn('IncraVegetable: could not load saves', err);
    removeRunDependency('incra-saves');
  });
});
