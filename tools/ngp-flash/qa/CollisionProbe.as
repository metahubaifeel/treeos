package {
    import flash.display.Sprite;
    import flash.display.BitmapData;
    import flash.geom.Matrix;
    import flash.geom.ColorTransform;
    import flash.geom.Rectangle;

    // Original fixture, no game code/assets/save. Test a known 4x5 overlap,
    // disjoint geometry, plus normal blending as a negative control.
    public class CollisionProbe extends Sprite {
        public function CollisionProbe() {
            sample("difference", 4);
            sample("difference", 12);
            sample("normal", 4);
        }
        private function sample(mode:String, dx:int):void {
            var pixels:BitmapData = new BitmapData(24, 12, false, 0);
            var box:Sprite = new Sprite();
            box.graphics.beginFill(0xffffff); box.graphics.drawRect(0, 0, 8, 5); box.graphics.endFill();
            pixels.draw(box, new Matrix(), new ColorTransform(1,1,1,1,255,-255,-255,255));
            pixels.draw(box, new Matrix(1,0,0,1,dx,0), new ColorTransform(1,1,1,1,255,255,255,255), mode);
            var bounds:Rectangle = pixels.getColorBoundsRect(0xffffffff, 0xff00ffff);
            trace("COLLISION", mode, dx, "red", pixels.getPixel32(1,2).toString(16), "overlap", pixels.getPixel32(5,2).toString(16), "cyanBounds", bounds.x,bounds.y,bounds.width,bounds.height);
            pixels.dispose();
        }
    }
}
