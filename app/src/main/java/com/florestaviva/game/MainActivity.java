package com.florestaviva.game;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Typeface;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.Button;
import android.widget.TextView;

import java.io.InputStream;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends Activity {
    static { System.loadLibrary("florestaviva"); }

    public static native void nativeCreate();
    public static native void nativeSetGraphicsQuality(int level);
    public static native String nativeVulkanInfo();
    public static native void nativeResize(int width, int height);
    public static native void nativeUploadTexture(int index, Bitmap bitmap);
    public static native void nativeInput(float sideways, float forward, float lookX, float lookY,
                                          boolean running, boolean collect);
    public static native void nativeRender(float frameTime);
    public static native String nativeStatus();
    public static native float[] nativeMapData();
    public static native String nativeSave();
    public static native void nativeLoad(String state);

    private GLSurfaceView surface;
    private InputOverlay overlay;
    private TextView status;
    private Button qualityButton;
    private volatile int graphicsQuality=1;

    private void fullscreen() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @Override public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        fullscreen();
        android.util.Log.i("FlorestaViva",nativeVulkanInfo());
        graphicsQuality=getSharedPreferences("floresta_settings",MODE_PRIVATE).getInt("graphicsQuality",1);

        surface = new GLSurfaceView(this);
        surface.setEGLContextClientVersion(3);
        surface.setEGLConfigChooser(8, 8, 8, 8, 24, 0);
        surface.setRenderer(new GameRenderer());
        surface.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);

        overlay = new InputOverlay();
        status = new TextView(this);
        status.setText("FLORESTA VIVA   |   Carregando a floresta...");
        status.setTextColor(Color.WHITE);
        status.setTextSize(15);
        status.setTypeface(Typeface.DEFAULT_BOLD);
        status.setShadowLayer(3, 1, 1, Color.BLACK);
        status.setPadding(dp(15),dp(10),dp(15),dp(10));
        status.setBackgroundColor(0x66091D16);
        status.setGravity(Gravity.CENTER);

        FrameLayout layout = new FrameLayout(this);
        layout.addView(surface, new FrameLayout.LayoutParams(-1,-1));
        layout.addView(overlay, new FrameLayout.LayoutParams(-1,-1));
        FrameLayout.LayoutParams header = new FrameLayout.LayoutParams(-2,-2,Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        header.topMargin = dp(10);
        layout.addView(status, header);
        qualityButton=new Button(this);
        qualityButton.setAllCaps(false);
        qualityButton.setTextSize(12);
        qualityButton.setTextColor(Color.WHITE);
        qualityButton.setBackgroundTintList(
            android.content.res.ColorStateList.valueOf(0xCC103B31));
        qualityButton.setText(graphicsLabel(graphicsQuality));
        FrameLayout.LayoutParams preset=new FrameLayout.LayoutParams(-2,dp(42),
            Gravity.TOP|Gravity.RIGHT);
        preset.topMargin=dp(5);preset.rightMargin=dp(9);
        layout.addView(qualityButton,preset);
        qualityButton.setOnClickListener(v->{
            graphicsQuality=(graphicsQuality+1)%3;
            qualityButton.setText(graphicsLabel(graphicsQuality));
            getSharedPreferences("floresta_settings",MODE_PRIVATE).edit()
                .putInt("graphicsQuality",graphicsQuality).apply();
            surface.queueEvent(()->nativeSetGraphicsQuality(graphicsQuality));
        });
        setContentView(layout);
    }

    private static String graphicsLabel(int level){
        return level==0?"Gráficos: ECO":(level==2?"Gráficos: ULTRA":"Gráficos: ALTA");
    }

    private int dp(float pixels) {return Math.round(pixels*getResources().getDisplayMetrics().density);}

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) fullscreen();
    }
    @Override protected void onPause() {
        if(surface!=null){
            surface.queueEvent(()->getSharedPreferences("floresta_save",MODE_PRIVATE)
                    .edit().putString("progress",nativeSave()).apply());
            surface.onPause();
        }
        super.onPause();
    }
    @Override protected void onResume() {super.onResume();if(surface!=null)surface.onResume();fullscreen();}

    private static final class Snapshot {
        float mx, mz, lx, ly;
        boolean sprint, collect;
    }

    private class GameRenderer implements GLSurfaceView.Renderer {
        private long lastFrame=0;
        private int frames=0;
        @Override public void onSurfaceCreated(GL10 unused, EGLConfig config) {
            nativeCreate();
            nativeSetGraphicsQuality(graphicsQuality);
            String saved=getSharedPreferences("floresta_save",MODE_PRIVATE).getString("progress",null);
            if(saved!=null)nativeLoad(saved);
            String[] files = {"grass.png","soil.png","moss.png","bark.png",
                    "canopy.png","blades.png","bush.png",
                    "grass_normal.png","soil_normal.png","moss_normal.png","bark_normal.png",
                    "leaves_atlas.png","leaves_normal.png",
                    "pine_atlas.png","pine_normal.png",
                    "pine_bark.png","pine_bark_normal.png",
                    "rock.png","rock_normal.png"};
            BitmapFactory.Options opts = new BitmapFactory.Options();
            opts.inScaled = false;
            opts.inPreferredConfig = Bitmap.Config.ARGB_8888;
            for(int i=0;i<files.length;i++) {
                try(InputStream stream=getAssets().open(files[i])) {
                    Bitmap bitmap=BitmapFactory.decodeStream(stream,null,opts);
                    if(bitmap!=null){nativeUploadTexture(i,bitmap);bitmap.recycle();}
                }catch(Exception e){android.util.Log.e("FlorestaViva","Failed texture: "+files[i],e);}
            }
            lastFrame=System.nanoTime();
        }
        @Override public void onSurfaceChanged(GL10 unused,int width,int height){nativeResize(width,height);}
        @Override public void onDrawFrame(GL10 unused) {
            long now=System.nanoTime();
            float dt=Math.min(.05f,(now-lastFrame)/1_000_000_000f);
            lastFrame=now;
            Snapshot input=overlay.snapshot();
            nativeInput(input.mx,input.mz,input.lx,input.ly,input.sprint,input.collect);
            nativeRender(dt);
            if((frames++%180)==0){
                getSharedPreferences("floresta_save",MODE_PRIVATE).edit()
                        .putString("progress",nativeSave()).apply();
            }
            if((frames%24)==0){
                String line=nativeStatus();
                float[] map=nativeMapData();
                runOnUiThread(()->{status.setText(line);overlay.setMap(map);});
            }
        }
    }

    private class InputOverlay extends View {
        private final Paint paint=new Paint(3);
        private final float density=getResources().getDisplayMetrics().density;
        private int stickId=-1, lookId=-1, sprintId=-1;
        private float anchorX=100*density,anchorY=300*density;
        private float stickX=0,stickY=0,lastLookX=0,lastLookY=0;
        private float pendingLookX=0,pendingLookY=0;
        private boolean picked=false;
        private float[] mapData=null;
        synchronized void setMap(float[] points){mapData=points;postInvalidateOnAnimation();}
        private final float radius=60*density;
        private final float buttonRadius=45*density;

        InputOverlay(){super(MainActivity.this);setLayerType(View.LAYER_TYPE_SOFTWARE,null);}
        private float sprintX(){return getWidth()-85*density;}
        private float sprintY(){return getHeight()-92*density;}
        private float pickupX(){return getWidth()-85*density;}
        private float pickupY(){return getHeight()-215*density;}
        private boolean near(float x,float y,float cx,float cy,float dist){
            return (x-cx)*(x-cx)+(y-cy)*(y-cy)<dist*dist;
        }
        synchronized Snapshot snapshot(){
            Snapshot s=new Snapshot();
            s.mx=stickX/radius;s.mz=-stickY/radius;
            s.lx=pendingLookX;s.ly=pendingLookY;
            s.sprint=sprintId!=-1;s.collect=picked;
            pendingLookX=0;pendingLookY=0;picked=false;
            return s;
        }
        @Override public synchronized boolean onTouchEvent(MotionEvent event) {
            int action=event.getActionMasked();
            int ai=event.getActionIndex();
            if(action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_POINTER_DOWN){
                int id=event.getPointerId(ai);
                float x=event.getX(ai),y=event.getY(ai);
                if(near(x,y,pickupX(),pickupY(),buttonRadius*1.28f))picked=true;
                else if(near(x,y,sprintX(),sprintY(),buttonRadius*1.30f))sprintId=id;
                else if(x<getWidth()*.45f && y>getHeight()*.32f && stickId==-1){
                    stickId=id;anchorX=x;anchorY=y;stickX=stickY=0;
                }else if(lookId==-1){lookId=id;lastLookX=x;lastLookY=y;}
            }
            if(action==MotionEvent.ACTION_MOVE){
                for(int i=0;i<event.getPointerCount();i++){
                    int id=event.getPointerId(i);
                    float x=event.getX(i),y=event.getY(i);
                    if(id==stickId){
                        float dx=x-anchorX,dy=y-anchorY;
                        float length=(float)Math.hypot(dx,dy);
                        float scale=length>radius?radius/length:1;
                        stickX=dx*scale;stickY=dy*scale;
                    }else if(id==lookId){
                        pendingLookX+=x-lastLookX;pendingLookY+=y-lastLookY;
                        lastLookX=x;lastLookY=y;
                    }
                }
            }
            if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP||action==MotionEvent.ACTION_CANCEL){
                int id=event.getPointerId(ai);
                if(id==stickId||action==MotionEvent.ACTION_CANCEL){stickId=-1;stickX=stickY=0;}
                if(id==lookId||action==MotionEvent.ACTION_CANCEL)lookId=-1;
                if(id==sprintId||action==MotionEvent.ACTION_CANCEL)sprintId=-1;
            }
            postInvalidateOnAnimation();return true;
        }
        private void circle(Canvas c,float x,float y,float rad,int fill){
            paint.setColor(fill);paint.setStyle(Paint.Style.FILL);c.drawCircle(x,y,rad,paint);
            paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(2*density);
            paint.setColor(0x99FFFFFF);c.drawCircle(x,y,rad,paint);paint.setStyle(Paint.Style.FILL);
        }
        private void label(Canvas c,String text,float x,float y,float size){
            paint.setColor(Color.WHITE);paint.setTypeface(Typeface.DEFAULT_BOLD);
            paint.setTextSize(size*density);paint.setTextAlign(Paint.Align.CENTER);
            paint.setShadowLayer(2*density,0,1*density,Color.BLACK);
            c.drawText(text,x,y,paint);paint.clearShadowLayer();
        }
        @Override protected synchronized void onDraw(Canvas c){
            super.onDraw(c);
            float sx=stickId==-1?95*density:anchorX;
            float sy=stickId==-1?getHeight()-105*density:anchorY;
            circle(c,sx,sy,radius,0x48303F33);
            circle(c,sx+stickX,sy+stickY,25*density,0xAA9FC4AA);
            circle(c,sprintX(),sprintY(),buttonRadius,
                    sprintId==-1?0x77405645:0xCC4F9A60);
            label(c,"CORRER",sprintX(),sprintY()+5*density,12);
            circle(c,pickupX(),pickupY(),buttonRadius,0x99678961);
            label(c,"PEGAR",pickupX(),pickupY()+5*density,13);
            paint.setTextAlign(Paint.Align.LEFT);
            paint.setColor(0xCCFFFFFF);
            paint.setTextSize(11*density);
            c.drawText("Mover",sx-radius,sy-radius-13*density,paint);
            c.drawText("Arraste no lado direito para olhar",getWidth()*.47f,getHeight()-18*density,paint);
            drawMinimap(c);
            postInvalidateOnAnimation();
        }
        private void drawMinimap(Canvas c){
            if(mapData==null)return;
            float centerX=90*density,centerY=102*density,r=64*density;
            circle(c,centerX,centerY,r,0xB3112C21);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(1*density);paint.setColor(0x66FFFFFF);
            c.drawLine(centerX-r,centerY,centerX+r,centerY,paint);
            c.drawLine(centerX,centerY-r,centerX,centerY+r,paint);
            paint.setStyle(Paint.Style.FILL);
            for(int i=3;i+1<mapData.length;i+=2){
                if(mapData[i]>90000)continue;
                float dx=(mapData[i]-mapData[0])*.35f*density;
                float dy=-(mapData[i+1]-mapData[1])*.35f*density;
                float len=(float)Math.hypot(dx,dy);
                float scale=len>r-8*density?(r-8*density)/len:1;
                paint.setColor(0xFFFFDB70);
                c.drawCircle(centerX+dx*scale,centerY+dy*scale,5*density,paint);
            }
            float yaw=mapData[2];
            float fx=(float)Math.sin(yaw),fy=-(float)Math.cos(yaw);
            float rx=-fy,ry=fx;
            Path arrow=new Path();
            arrow.moveTo(centerX+fx*11*density,centerY+fy*11*density);
            arrow.lineTo(centerX-fx*6*density+rx*6*density,centerY-fy*6*density+ry*6*density);
            arrow.lineTo(centerX-fx*6*density-rx*6*density,centerY-fy*6*density-ry*6*density);
            arrow.close();paint.setColor(0xFFFFFFFF);c.drawPath(arrow,paint);
            label(c,"MAPA  •  180 m",centerX,centerY+r+20*density,10);
        }
    }
}
